// SPDX-License-Identifier: GPL-2.0-or-later
// Runs the yabi binary against fake_fbv and checks what fbv was asked.
// Usage: cli_test <yabi> <fake_fbv> <themes dir>
#include "check.hpp"

#include <array>
#include <cstdio>
#include <fstream>
#include <string>
#include <sys/stat.h>
#include <sys/wait.h>
#include <thread>
#include <unistd.h>

namespace {

std::string yabi, fake, themes, dir, sock, log_path;

struct Result {
	int status;
	std::string out;
};

// Runs yabi with the given arguments and environment prefix.
Result run(const std::string &args, const std::string &env = {})
{
	const std::string cmd = env + " YABI_FBV=" + fake + " FAKE_FBV_LOG=" + log_path + " " + yabi +
				" -s " + sock + " -d " + themes + " -t 2000 " + args + " 2>&1";
	Result r{-1, {}};
	FILE *p = ::popen(cmd.c_str(), "r");
	if (!p)
		return r;
	std::array<char, 256> buf;
	while (std::fgets(buf.data(), buf.size(), p))
		r.out += buf.data();
	const int status = ::pclose(p);
	r.status = WIFEXITED(status) ? WEXITSTATUS(status) : -1;
	return r;
}

std::string logged()
{
	std::ifstream in(log_path);
	return {std::istreambuf_iterator<char>(in), {}};
}

void reset_log()
{
	std::ofstream(log_path, std::ios::trunc);
}

} // namespace

int main(int argc, char **argv)
{
	if (argc < 4) {
		std::fprintf(stderr, "usage: %s <yabi> <fake_fbv> <themes>\n", argv[0]);
		return 2;
	}
	yabi = argv[1];
	fake = argv[2];
	themes = argv[3];
	char tmp[] = "/tmp/yabi-cli-XXXXXX";
	if (!::mkdtemp(tmp))
		return 2;
	dir = tmp;
	sock = dir + "/yabi.sock";
	log_path = dir + "/log";

	// Nothing running yet
	CHECK(run("ping").status == 3);
	CHECK(run("progress 10").status == 3);
	CHECK(run("quit").status == 3);

	// Usage errors
	CHECK(run("").status == 2);
	CHECK(run("dance").status == 2);
	CHECK(run("progress").status == 2);
	CHECK(run("start a b").status == 2);
	CHECK(run("-t 0 ping").status == 2);
	CHECK(run("--help").status == 0);
	CHECK(run("--version").out.rfind("yabi ", 0) == 0);

	CHECK(run("start nosuchtheme").status == 1);
	CHECK(run("start", "FAKE_FBV_FAIL=1").status == 1);
	CHECK(run("-f /nonexistent/fbv start").status == 1);

	// start launches fbv with the theme and holds its first frame.
	reset_log();
	Result r = run("start");
	CHECK(r.status == 0);
	const std::string anim = themes + "/default/progress.gif";
	char real[4096];
	CHECK(::realpath(anim.c_str(), real));
	CHECK(logged() == "args -y -c -i -u -g --control " + sock + " " + real + "\nping\nframe 0\n");
	CHECK(run("start").status == 1);

	reset_log();
	CHECK(run("progress 42").status == 0);
	CHECK(run("status").out == "frame=42 frames=101 paused=0\n");
	CHECK(run("pause").status == 0);
	CHECK(run("resume").status == 0);
	CHECK(run("frame 7").status == 0);
	CHECK(run("show " + anim).status == 0);
	r = run("show " + themes + "/missing.gif");
	CHECK(r.status == 1);
	CHECK(run("theme default").status == 0);
	CHECK(logged() == "progress 42\nstatus\npause\nresume\nframe 7\nload " + std::string(real) +
				  "\nload " + real + "\nframe 0\n");

	// rc moves the progress around the scripts.
	const std::string rc = dir + "/init.d";
	::mkdir(rc.c_str(), 0755);
	for (const char *name : {"S10a", "S20b"}) {
		std::ofstream(rc + "/" + name) << "#!/bin/sh\nexit 0\n";
		::chmod((rc + "/" + name).c_str(), 0755);
	}
	reset_log();
	CHECK(run("rc " + rc).status == 0);
	CHECK(logged() == "progress 0\nprogress 50\nprogress 100\n");

	// quit returns once fbv has gone.
	reset_log();
	CHECK(run("--retain quit").status == 0);
	CHECK(logged() == "quit retain\n");
	CHECK(::access(sock.c_str(), F_OK) != 0);
	CHECK(run("ping").status == 3);

	// Graphics mode can be left off.
	reset_log();
	CHECK(run("--no-graphics start").status == 0);
	CHECK(logged().rfind("args -y -c -i -u --control ", 0) == 0);
	CHECK(run("quit").status == 0);

	(void)!std::system(("rm -rf " + dir).c_str());
	return check_result();
}
