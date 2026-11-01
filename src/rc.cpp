// SPDX-License-Identifier: GPL-2.0-or-later
#include "rc.hpp"

#include <algorithm>
#include <cerrno>
#include <cstdio>
#include <dirent.h>
#include <memory>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>

namespace yabi {

std::vector<std::string> rc_scripts(const std::string &dir)
{
	std::unique_ptr<DIR, int (*)(DIR *)> d(::opendir(dir.c_str()), ::closedir);
	std::vector<std::string> out;
	if (!d)
		return out;
	while (const dirent *e = ::readdir(d.get())) {
		const std::string name = e->d_name;
		if (name.size() < 2 || name[0] != 'S' || name.back() == '~')
			continue;
		const std::string path = dir + "/" + name;
		struct stat st;
		if (::stat(path.c_str(), &st) == 0 && S_ISREG(st.st_mode) && (st.st_mode & S_IXUSR))
			out.push_back(path);
	}
	std::sort(out.begin(), out.end());
	return out;
}

int run_rc(const std::vector<std::string> &scripts, const std::function<void(int)> &progress)
{
	int failed = 0;
	const auto total = static_cast<int>(scripts.size());
	for (int i = 0; i < total; i++) {
		progress(100 * i / total);
		const std::string &script = scripts[static_cast<std::size_t>(i)];
		const pid_t pid = ::fork();
		if (pid < 0) {
			std::perror("fork");
			failed++;
			continue;
		}
		if (pid == 0) {
			::execl(script.c_str(), script.c_str(), "start", static_cast<char *>(nullptr));
			::_exit(127);
		}
		int status = 0;
		while (::waitpid(pid, &status, 0) < 0 && errno == EINTR)
			;
		if (!WIFEXITED(status) || WEXITSTATUS(status)) {
			std::fprintf(stderr, "yabi: %s failed\n", script.c_str());
			failed++;
		}
	}
	progress(100);
	return failed;
}

} // namespace yabi
