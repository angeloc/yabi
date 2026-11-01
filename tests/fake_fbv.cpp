// SPDX-License-Identifier: GPL-2.0-or-later
// Stand-in for fbv: serves the control protocol on --control <path> to any
// number of clients at once and appends its arguments and every command to
// $FAKE_FBV_LOG. FAKE_FBV_FAIL=1 makes it exit at once.
#include "unique_fd.hpp"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <poll.h>
#include <string>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#include <vector>

namespace {

struct Client {
	yabi::UniqueFd fd;
	std::string buffer;
};

} // namespace

int main(int argc, char **argv)
{
	if (std::getenv("FAKE_FBV_FAIL"))
		return 1;
	const char *log_path = std::getenv("FAKE_FBV_LOG");
	std::ofstream log(log_path ? log_path : "/dev/null", std::ios::app);

	std::string socket_path;
	log << "args";
	for (int i = 1; i < argc; i++) {
		log << ' ' << argv[i];
		if (!std::strcmp(argv[i], "--control") && i + 1 < argc)
			socket_path = argv[i + 1];
	}
	log << std::endl;

	sockaddr_un addr{};
	addr.sun_family = AF_UNIX;
	std::strncpy(addr.sun_path, socket_path.c_str(), sizeof(addr.sun_path) - 1);
	yabi::UniqueFd srv(::socket(AF_UNIX, SOCK_STREAM, 0));
	::unlink(socket_path.c_str());
	if (!srv || ::bind(srv.get(), reinterpret_cast<sockaddr *>(&addr), sizeof(addr)) < 0 ||
	    ::listen(srv.get(), 8) < 0)
		return 1;

	unsigned frame = 0;
	bool paused = false;
	std::vector<Client> clients;
	for (;;) {
		std::vector<pollfd> fds{{srv.get(), POLLIN, 0}};
		for (const auto &c : clients)
			fds.push_back({c.fd.get(), POLLIN, 0});
		if (::poll(fds.data(), fds.size(), -1) < 0)
			continue;
		if (fds[0].revents)
			clients.push_back({yabi::UniqueFd(::accept(srv.get(), nullptr, nullptr)), {}});

		for (std::size_t i = 1; i < fds.size(); i++) {
			if (!fds[i].revents)
				continue;
			Client &c = clients[i - 1];
			char chunk[256];
			const ssize_t n = ::read(c.fd.get(), chunk, sizeof(chunk));
			if (n <= 0) {
				c.fd.reset();
				continue;
			}
			c.buffer.append(chunk, static_cast<std::size_t>(n));
			for (std::size_t eol; (eol = c.buffer.find('\n')) != std::string::npos;) {
				const std::string line = c.buffer.substr(0, eol);
				c.buffer.erase(0, eol + 1);
				log << line << std::endl;
				std::string reply = "ok";
				// Like fbv: progress, frame and resume run the animation again.
				if (line.rfind("progress ", 0) == 0) {
					frame = static_cast<unsigned>(std::atoi(line.c_str() + 9));
					paused = false;
				} else if (line == "pause") {
					paused = true;
				} else if (line == "resume" || line.rfind("frame ", 0) == 0) {
					paused = false;
				} else if (line == "status") {
					reply = "ok frame=" + std::to_string(frame) + " frames=101 paused=" +
						(paused ? "1" : "0");
				} else if (line.rfind("load ", 0) == 0 && line.find("missing") != std::string::npos) {
					reply = "error cannot load";
				}
				reply += "\n";
				(void)!::write(c.fd.get(), reply.data(), reply.size());
				if (line.rfind("quit", 0) == 0) {
					::unlink(socket_path.c_str());
					return 0;
				}
			}
		}
		std::erase_if(clients, [](const Client &c) { return !c.fd; });
	}
}
