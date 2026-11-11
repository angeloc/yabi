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
	CHECK(run("watch").status == 3);

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
	CHECK(logged() == "args -y -c -i -u -p -g --control " + sock + " " + real + "\nping\nframe 0\n");
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

	// watch follows the init scripts as they run, and ends with the splash.
	const std::string initd = dir + "/init.d";
	::mkdir(initd.c_str(), 0755);
	for (const char *name : {"S10a", "S20b", "S30c"}) {
		std::ofstream(initd + "/" + name) << "#!/bin/sh\nexit 0\n";
		::chmod((initd + "/" + name).c_str(), 0755);
	}
	reset_log();
	CHECK(run("watch " + initd).status == 0);
	CHECK(std::system((initd + "/S20b start").c_str()) == 0);
	bool seen = false;
	for (int i = 0; i < 100 && !seen; i++) {
		seen = logged() == "status\nprogress 33\n";
		if (!seen)
			std::this_thread::sleep_for(std::chrono::milliseconds(20));
	}
	CHECK(seen);
	CHECK(std::system("pgrep -f '[y]abi -s .* watch /' >/dev/null") == 0);

	// Once the splash is paused for the application, the watcher stops.
	CHECK(run("pause").status == 0);
	reset_log();
	CHECK(std::system((initd + "/S30c start").c_str()) == 0);
	for (int i = 0; i < 50 && std::system("pgrep -f '[y]abi -s .* watch /' >/dev/null") == 0; i++)
		std::this_thread::sleep_for(std::chrono::milliseconds(20));
	CHECK(logged() == "status\n");
	CHECK(std::system("pgrep -f '[y]abi -s .* watch /' >/dev/null") != 0);
	CHECK(run("resume").status == 0);

	// quit returns once fbv has gone.
	reset_log();
	CHECK(run("--retain quit").status == 0);
	CHECK(logged() == "quit retain\n");
	CHECK(::access(sock.c_str(), F_OK) != 0);
	CHECK(run("ping").status == 3);
	// The watcher ended with the splash.
	std::this_thread::sleep_for(std::chrono::milliseconds(200));
	CHECK(std::system("pgrep -f '[y]abi -s .* watch /' >/dev/null") != 0);

	// Graphics mode can be left off.
	reset_log();
	CHECK(run("--no-graphics start").status == 0);
	CHECK(logged().rfind("args -y -c -i -u -p --control ", 0) == 0);
	CHECK(run("quit").status == 0);

	// A loop theme spins on whatever the progress says.
	reset_log();
	CHECK(run("--no-graphics start theme3").status == 0);
	CHECK(logged().find("ping\nresume\n") != std::string::npos);
	reset_log();
	CHECK(run("progress 40").status == 0);
	CHECK(run("watch " + initd).status == 0);
	CHECK(run("freeze").status == 0);
	CHECK(logged() == "pause\n");
	CHECK(run("quit").status == 0);
	CHECK(::access((sock + ".mode").c_str(), F_OK) != 0);

	// A progress theme freezes at 100%.
	CHECK(run("--no-graphics start").status == 0);
	reset_log();
	CHECK(run("freeze").status == 0);
	CHECK(logged() == "progress 100\nstatus\npause\n");
	CHECK(::access((sock + ".frozen").c_str(), F_OK) == 0);
	CHECK(run("quit").status == 0);
	CHECK(::access((sock + ".frozen").c_str(), F_OK) != 0);

	// While freeze takes the progress to 100%, the watcher moves it no more.
	CHECK(run("--no-graphics start").status == 0);
	CHECK(run("watch " + initd).status == 0);
	std::ofstream(sock + ".frozen") << '\n';
	reset_log();
	CHECK(std::system((initd + "/S30c start").c_str()) == 0);
	for (int i = 0; i < 50 && std::system("pgrep -f '[y]abi -s .* watch /' >/dev/null") == 0; i++)
		std::this_thread::sleep_for(std::chrono::milliseconds(20));
	CHECK(std::system("pgrep -f '[y]abi -s .* watch /' >/dev/null") != 0);
	CHECK(logged() == "status\n");
	CHECK(run("quit").status == 0);

	(void)!std::system(("rm -rf " + dir).c_str());
	return check_result();
}
