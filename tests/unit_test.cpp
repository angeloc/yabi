// SPDX-License-Identifier: GPL-2.0-or-later
// Theme parsing and the rc script runner.
#include "check.hpp"
#include "rc.hpp"
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

void test_rc()
{
	const std::string rc = dir + "/init.d";
	::mkdir(rc.c_str(), 0755);
	const std::string out = dir + "/ran";
	auto script = [&](const std::string &name, const std::string &body, mode_t mode = 0755) {
		std::ofstream(rc + "/" + name) << "#!/bin/sh\necho \"" << name << " $1\" >> " << out << "\n" << body;
		::chmod((rc + "/" + name).c_str(), mode);
	};
	script("S20second", "");
	script("S10first", "");
	script("S30fails", "exit 3\n");
	script("S40skipped", "", 0644);
	script("K10stop", "");
	script("S50backup~", "");

	const auto scripts = rc_scripts(rc);
	CHECK(scripts == (std::vector<std::string>{rc + "/S10first", rc + "/S20second", rc + "/S30fails"}));

	std::vector<int> seen;
	CHECK(run_rc(scripts, [&](int p) { seen.push_back(p); }) == 1);
	CHECK(seen == (std::vector<int>{0, 33, 66, 100}));

	std::ifstream in(out);
	std::string ran((std::istreambuf_iterator<char>(in)), {});
	CHECK(ran == "S10first start\nS20second start\nS30fails start\n");

	seen.clear();
	CHECK(rc_scripts(dir + "/none").empty());
	CHECK(run_rc({}, [&](int p) { seen.push_back(p); }) == 0);
	CHECK(seen == (std::vector<int>{100}));
}

} // namespace

int main()
{
	char tmp[] = "/tmp/yabi-unit-XXXXXX";
	if (!::mkdtemp(tmp))
		return 2;
	dir = tmp;
	test_theme();
	test_rc();
	(void)!std::system(("rm -rf " + dir).c_str());
	return check_result();
}
