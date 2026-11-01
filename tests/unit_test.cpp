// SPDX-License-Identifier: GPL-2.0-or-later
// Theme parsing and the rc script runner.
#include "check.hpp"
#include "watch.hpp"
#include "theme.hpp"

#include <fstream>
#include <limits.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <unistd.h>

using namespace yabi;

namespace {

std::string dir;

void write(const std::string &name, const std::string &text, mode_t mode = 0644)
{
	std::ofstream(dir + "/" + name) << text;
	::chmod((dir + "/" + name).c_str(), mode);
}

Theme theme(const std::string &conf)
{
	write("theme.conf", conf);
	return load_theme(dir);
}

std::string real(const std::string &path)
{
	char buf[PATH_MAX];
	return ::realpath(path.c_str(), buf) ? buf : "";
}

void test_theme()
{
	write("anim.gif", "x");

	Theme t = theme("# splash\nanimation = anim.gif\n");
	CHECK(t.animation == real(dir + "/anim.gif"));
	CHECK(t.mode == Theme::Mode::progress && t.scale == Theme::Scale::none);

	t = theme("  animation=anim.gif  # trailing\n\nmode = loop\nscale = fit\n");
	CHECK(t.mode == Theme::Mode::loop && t.scale == Theme::Scale::fit);
	CHECK(theme("animation = " + dir + "/anim.gif\nscale = shrink\n").scale == Theme::Scale::shrink);

	CHECK_THROWS(ThemeError, theme(""));
	CHECK_THROWS(ThemeError, theme("animation = missing.gif\n"));
	CHECK_THROWS(ThemeError, theme("animation = anim.gif\nmode = spin\n"));
	CHECK_THROWS(ThemeError, theme("animation = anim.gif\nscale = huge\n"));
	CHECK_THROWS(ThemeError, theme("animation = anim.gif\ncolor = red\n"));
	CHECK_THROWS(ThemeError, theme("animation anim.gif\n"));
	CHECK_THROWS(ThemeError, theme("animation =\n"));
	CHECK_THROWS(ThemeError, theme("animation = anim.gif\n# " + std::string(2000, 'x') + "\n"));
	CHECK_THROWS(ThemeError, load_theme(dir + "/nowhere"));

	try {
		theme("animation = anim.gif\n\nmode = spin\n");
	} catch (const ThemeError &e) {
		CHECK(std::string(e.what()).find("theme.conf:3:") != std::string::npos);
	}

	CHECK(theme_directory("default", "/usr/share/yabi/themes") == "/usr/share/yabi/themes/default");
	CHECK(theme_directory("./mine", "/x") == "./mine");
	CHECK(theme_directory("/opt/t", "/x") == "/opt/t");
	CHECK_THROWS(ThemeError, theme_directory("..", "/x"));
	CHECK_THROWS(ThemeError, theme_directory("", "/x"));
}

void test_watch()
{
	const std::string init = dir + "/init.d", rc2 = dir + "/rc2.d";
	::mkdir(init.c_str(), 0755);
	::mkdir(rc2.c_str(), 0755);
	auto script = [&](const std::string &path, mode_t mode = 0755) {
		std::ofstream(path) << "#!/bin/sh\n";
		::chmod(path.c_str(), mode);
	};
	script(init + "/S20second");
	script(init + "/S10first");
	script(init + "/S30third");
	script(init + "/S40skipped", 0644);
	script(init + "/K10stop");
	script(init + "/S50backup~");
	script(init + "/late");
	CHECK(::symlink("../init.d/late", (rc2 + "/S90late").c_str()) == 0);

	const auto scripts = boot_scripts({init, rc2});
	CHECK(scripts == (std::vector<std::string>{init + "/S10first", init + "/S20second", init + "/S30third",
						   rc2 + "/S90late"}));
	CHECK(boot_scripts({dir + "/none"}).empty());

	BootWatcher boot({init, rc2});
	CHECK(!boot.progress());
	std::ifstream(init + "/S20second").get();
	CHECK(boot.progress() == 25);
	// Earlier scripts, and files that are no boot script, change nothing.
	std::ifstream(init + "/S10first").get();
	std::ifstream(init + "/K10stop").get();
	CHECK(boot.progress() == 25);
	// A runlevel link is followed to the file it names.
	std::ifstream(init + "/late").get();
	CHECK(boot.progress() == 75);
}

void test_openrc()
{
	const std::string root = dir + "/openrc";
	for (const char *d : {"", "/etc", "/etc/runlevels", "/etc/runlevels/boot", "/etc/runlevels/default",
			      "/run", "/run/openrc", "/run/openrc/started"})
		::mkdir((root + d).c_str(), 0755);
	for (const char *s : {"boot/hostname", "boot/syslog", "default/network", "default/app"})
		std::ofstream(root + "/etc/runlevels/" + s);

	CHECK(!openrc_progress(dir + "/nothing"));
	CHECK(openrc_progress(root) == 0);
	BootWatcher boot({}, root);
	std::ofstream(root + "/run/openrc/started/hostname");
	std::ofstream(root + "/run/openrc/started/unrelated");
	CHECK(boot.progress() == 25);
	std::ofstream(root + "/run/openrc/started/network");
	CHECK(boot.progress() == 50);
}

} // namespace

int main()
{
	char tmp[] = "/tmp/yabi-unit-XXXXXX";
	if (!::mkdtemp(tmp))
		return 2;
	dir = tmp;
	test_theme();
	test_watch();
	test_openrc();
	(void)!std::system(("rm -rf " + dir).c_str());
	return check_result();
}
