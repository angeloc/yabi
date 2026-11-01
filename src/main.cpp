// SPDX-License-Identifier: GPL-2.0-or-later
#include "client.hpp"
#include "launch.hpp"
#include "rc.hpp"
#include "theme.hpp"

#include <cerrno>
#include <climits>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <getopt.h>
#include <stdlib.h>
#include <string>
#include <thread>
#include <vector>

using namespace yabi;
using namespace std::chrono_literals;

namespace {

enum Exit { success = 0, failure = 1, usage_error = 2, not_running = 3 };

struct Settings {
	std::string socket = YABI_DEFAULT_SOCKET;
	std::string fbv = "fbv";
	std::string themes = YABI_THEME_DIR;
	std::chrono::milliseconds timeout = 3000ms;
	bool graphics = true;
	bool retain = false;
};

void usage(FILE *out)
{
	std::fprintf(out,
		"Usage: yabi [options] <command> [arguments]\n\n"
		"Commands:\n"
		"  start [theme]     show the splash: start fbv with the theme (default: default)\n"
		"  progress <0-100>  move the progress animation\n"
		"  frame <n>         show frame n of the animation\n"
		"  pause, resume     stop and restart the animation\n"
		"  show <file>       switch to another image or animation\n"
		"  theme <theme>     switch to another theme\n"
		"  status            print the frame, frame count and pause state\n"
		"  ping              exit 0 when the splash is running\n"
		"  quit              close the splash and wait until fbv exited\n"
		"  rc [dir]          run the S??* scripts of dir (default /etc/init.d) with \"start\",\n"
		"                    moving the progress after each one\n\n"
		"Options:\n"
		"  -s, --socket <path>  control socket (default %s, or $YABI_SOCKET)\n"
		"  -f, --fbv <path>     fbv to start (default fbv from $PATH, or $YABI_FBV)\n"
		"  -d, --themes <dir>   theme directory (default %s)\n"
		"  -t, --timeout <ms>   how long to wait for fbv (default 3000)\n"
		"  -n, --no-graphics    start: leave the console in text mode\n"
		"  -r, --retain         quit: keep the splash and graphics mode for the next program\n"
		"  -h, --help           show this help\n"
		"  -V, --version        show the version\n\n"
		"Exit status: 0 done, 1 failed, 2 usage error, 3 splash not running.\n",
		YABI_DEFAULT_SOCKET, YABI_THEME_DIR);
}

std::string absolute(const std::string &path)
{
	char resolved[PATH_MAX];
	if (!::realpath(path.c_str(), resolved))
		throw std::runtime_error(path + ": " + std::strerror(errno));
	return resolved;
}

// Sends one command to the running splash.
int send(const Settings &s, const std::string &command, bool print = false)
{
	auto client = Client::connect(s.socket);
	if (!client) {
		std::fprintf(stderr, "yabi: splash not running (%s)\n", s.socket.c_str());
		return not_running;
	}
	const Reply reply = client->request(command, s.timeout);
	if (!reply.ok) {
		std::fprintf(stderr, "yabi: %s\n", reply.text.c_str());
		return failure;
	}
	if (print && !reply.text.empty())
		std::printf("%s\n", reply.text.c_str());
	return success;
}

// Puts a freshly loaded theme in its initial state.
int apply_mode(const Settings &s, const Theme &theme)
{
	return send(s, theme.mode == Theme::Mode::progress ? "frame 0" : "resume");
}

int start(const Settings &s, const std::string &name)
{
	if (Client::connect(s.socket)) {
		std::fprintf(stderr, "yabi: splash already running (%s)\n", s.socket.c_str());
		return failure;
	}
	const Theme theme = load_theme(theme_directory(name, s.themes));

	std::vector<std::string> args = {s.fbv, "-y", "-c", "-i", "-u"};
	if (s.graphics)
		args.emplace_back("-g");
	if (theme.scale == Theme::Scale::shrink)
		args.emplace_back("-f");
	else if (theme.scale == Theme::Scale::fit)
		args.insert(args.end(), {"-f", "-e"});
	args.insert(args.end(), {"--control", s.socket, theme.animation});

	const pid_t pid = launch(args);
	const auto deadline = std::chrono::steady_clock::now() + s.timeout;
	for (;;) {
		if (auto client = Client::connect(s.socket))
			if (client->request("ping", s.timeout).ok)
				break;
		if (!running(pid)) {
			std::fprintf(stderr, "yabi: %s exited, run it by hand to see why\n", s.fbv.c_str());
			return failure;
		}
		if (std::chrono::steady_clock::now() > deadline) {
			std::fprintf(stderr, "yabi: %s did not open %s\n", s.fbv.c_str(), s.socket.c_str());
			return failure;
		}
		std::this_thread::sleep_for(20ms);
	}
	return apply_mode(s, theme);
}

int quit(const Settings &s)
{
	auto client = Client::connect(s.socket);
	if (!client) {
		std::fprintf(stderr, "yabi: splash not running (%s)\n", s.socket.c_str());
		return not_running;
	}
	const Reply reply = client->request(s.retain ? "quit retain" : "quit", s.timeout);
	if (!reply.ok) {
		std::fprintf(stderr, "yabi: %s\n", reply.text.c_str());
		return failure;
	}
	// The framebuffer is free only once fbv is gone.
	if (!client->wait_closed(s.timeout)) {
		std::fprintf(stderr, "yabi: fbv did not exit\n");
		return failure;
	}
	return success;
}

int rc(const Settings &s, const std::string &dir)
{
	const auto scripts = rc_scripts(dir);
	const int failed = run_rc(scripts, [&](int percent) {
		// The splash may be gone or disabled; booting goes on anyway.
		if (auto client = Client::connect(s.socket)) {
			try {
				client->request("progress " + std::to_string(percent), s.timeout);
			} catch (const std::exception &) {
			}
		}
	});
	return failed ? failure : success;
}

bool parse_ms(const char *text, std::chrono::milliseconds &out)
{
	char *end;
	errno = 0;
	const long v = std::strtol(text, &end, 10);
	if (errno || end == text || *end || v < 1 || v > 600000)
		return false;
	out = std::chrono::milliseconds(v);
	return true;
}

} // namespace

int main(int argc, char **argv)
{
	Settings s;
	if (const char *env = std::getenv("YABI_SOCKET"))
		s.socket = env;
	if (const char *env = std::getenv("YABI_FBV"))
		s.fbv = env;

	static const option options[] = {
		{"socket", required_argument, nullptr, 's'},
		{"fbv", required_argument, nullptr, 'f'},
		{"themes", required_argument, nullptr, 'd'},
		{"timeout", required_argument, nullptr, 't'},
		{"no-graphics", no_argument, nullptr, 'n'},
		{"retain", no_argument, nullptr, 'r'},
		{"help", no_argument, nullptr, 'h'},
		{"version", no_argument, nullptr, 'V'},
		{nullptr, 0, nullptr, 0},
	};
	int c;
	while ((c = getopt_long(argc, argv, "+s:f:d:t:nrhV", options, nullptr)) != -1) {
		switch (c) {
		case 's': s.socket = optarg; break;
		case 'f': s.fbv = optarg; break;
		case 'd': s.themes = optarg; break;
		case 't':
			if (!parse_ms(optarg, s.timeout)) {
				std::fprintf(stderr, "yabi: invalid timeout %s\n", optarg);
				return usage_error;
			}
			break;
		case 'n': s.graphics = false; break;
		case 'r': s.retain = true; break;
		case 'h': usage(stdout); return success;
		case 'V': std::printf("yabi %s\n", YABI_VERSION); return success;
		default: usage(stderr); return usage_error;
		}
	}

	const std::vector<std::string> args(argv + optind, argv + argc);
	if (args.empty()) {
		usage(stderr);
		return usage_error;
	}
	const std::string &cmd = args[0];
	auto arity = [&](std::size_t min, std::size_t max) {
		if (args.size() - 1 < min || args.size() - 1 > max) {
			std::fprintf(stderr, "yabi: wrong number of arguments for %s\n", cmd.c_str());
			return false;
		}
		return true;
	};

	try {
		if (cmd == "start")
			return arity(0, 1) ? start(s, args.size() > 1 ? args[1] : "default") : usage_error;
		if (cmd == "progress" || cmd == "frame")
			return arity(1, 1) ? send(s, cmd + " " + args[1]) : usage_error;
		if (cmd == "pause" || cmd == "resume" || cmd == "ping")
			return arity(0, 0) ? send(s, cmd) : usage_error;
		if (cmd == "status")
			return arity(0, 0) ? send(s, cmd, true) : usage_error;
		if (cmd == "show")
			return arity(1, 1) ? send(s, "load " + absolute(args[1])) : usage_error;
		if (cmd == "theme") {
			if (!arity(1, 1))
				return usage_error;
			const Theme theme = load_theme(theme_directory(args[1], s.themes));
			const int rc = send(s, "load " + theme.animation);
			return rc ? rc : apply_mode(s, theme);
		}
		if (cmd == "quit")
			return arity(0, 0) ? quit(s) : usage_error;
		if (cmd == "rc")
			return arity(0, 1) ? rc(s, args.size() > 1 ? args[1] : "/etc/init.d") : usage_error;
	} catch (const std::exception &e) {
		std::fprintf(stderr, "yabi: %s\n", e.what());
		return failure;
	}
	std::fprintf(stderr, "yabi: unknown command %s\n", cmd.c_str());
	usage(stderr);
	return usage_error;
}
