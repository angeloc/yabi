// SPDX-License-Identifier: GPL-2.0-or-later
#include "watch.hpp"

#include <algorithm>
#include <cerrno>
#include <climits>
#include <dirent.h>
#include <memory>
#include <set>
#include <stdlib.h>
#include <sys/inotify.h>
#include <sys/stat.h>
#include <system_error>
#include <unistd.h>

namespace yabi {

namespace {

std::vector<std::string> entries(const std::string &dir)
{
	std::unique_ptr<DIR, int (*)(DIR *)> d(::opendir(dir.c_str()), ::closedir);
	std::vector<std::string> out;
	if (!d)
		return out;
	while (const dirent *e = ::readdir(d.get())) {
		const std::string name = e->d_name;
		if (name != "." && name != "..")
			out.push_back(name);
	}
	std::sort(out.begin(), out.end());
	return out;
}

bool is_directory(const std::string &path)
{
	struct stat st;
	return ::stat(path.c_str(), &st) == 0 && S_ISDIR(st.st_mode);
}

} // namespace

std::vector<std::string> boot_scripts(const std::vector<std::string> &dirs)
{
	std::vector<std::string> out;
	for (const auto &dir : dirs)
		for (const auto &name : entries(dir)) {
			if (name.size() < 2 || name[0] != 'S' || name.back() == '~')
				continue;
			const std::string path = dir + "/" + name;
			struct stat st;
			if (::stat(path.c_str(), &st) == 0 && S_ISREG(st.st_mode) && (st.st_mode & S_IXUSR))
				out.push_back(path);
		}
	return out;
}

std::optional<int> openrc_progress(const std::string &root)
{
	const std::string started = root + "/run/openrc/started";
	if (!is_directory(started))
		return std::nullopt;

	std::set<std::string> services;
	for (const char *level : {"sysinit", "boot", "default"})
		for (const auto &name : entries(root + "/etc/runlevels/" + level))
			services.insert(name);
	if (services.empty())
		return std::nullopt;

	std::size_t done = 0;
	for (const auto &name : entries(started))
		done += services.count(name);
	return static_cast<int>(100 * done / services.size());
}

BootWatcher::BootWatcher(const std::vector<std::string> &dirs, const std::string &root)
	: fd_(::inotify_init1(IN_NONBLOCK | IN_CLOEXEC)), root_(root)
{
	if (!fd_)
		throw std::system_error(errno, std::generic_category(), "inotify");

	const std::string started = root_ + "/run/openrc/started";
	if (is_directory(started)) {
		openrc_ = true;
		::inotify_add_watch(fd_.get(), started.c_str(), IN_CREATE | IN_MOVED_TO | IN_DELETE);
		return;
	}

	// An open is reported on the directory holding the file itself, so
	// runlevel links like rc2.d/S20foo -> ../init.d/foo are resolved.
	const auto scripts = boot_scripts(dirs);
	scripts_ = scripts.size();
	std::map<std::string, int> watches;
	for (std::size_t i = 0; i < scripts.size(); i++) {
		char real[PATH_MAX];
		if (!::realpath(scripts[i].c_str(), real))
			continue;
		const std::string path = real;
		const auto slash = path.rfind('/');
		const std::string dir = path.substr(0, slash ? slash : 1);
		auto it = watches.find(dir);
		if (it == watches.end()) {
			const int wd = ::inotify_add_watch(fd_.get(), dir.c_str(), IN_OPEN);
			if (wd < 0)
				continue;
			it = watches.emplace(dir, wd).first;
		}
		// The first script reaching a file sets its place.
		index_.emplace(std::make_pair(it->second, path.substr(slash + 1)), i);
	}
}

std::optional<int> BootWatcher::progress()
{
	alignas(inotify_event) char buf[4096];
	ssize_t n;
	while ((n = ::read(fd_.get(), buf, sizeof(buf))) > 0) {
		for (char *p = buf; p < buf + n;) {
			const auto *ev = reinterpret_cast<const inotify_event *>(p);
			if (!openrc_ && ev->len) {
				const auto it = index_.find({ev->wd, ev->name});
				if (it != index_.end())
					latest_ = std::max(latest_.value_or(0), it->second);
			}
			p += sizeof(inotify_event) + ev->len;
		}
	}
	if (openrc_)
		return openrc_progress(root_);
	if (!latest_ || !scripts_)
		return std::nullopt;
	return static_cast<int>(100 * *latest_ / scripts_);
}

} // namespace yabi
