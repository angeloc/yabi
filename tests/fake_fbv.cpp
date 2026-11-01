// SPDX-License-Identifier: GPL-2.0-or-later
// Stand-in for fbv: serves the control protocol on --control <path> and
// appends its arguments and every command to $FAKE_FBV_LOG.
// FAKE_FBV_FAIL=1 makes it exit at once.
#include "unique_fd.hpp"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <string>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

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
	    ::listen(srv.get(), 4) < 0)
		return 1;

	unsigned frame = 0;
	for (;;) {
		yabi::UniqueFd client(::accept(srv.get(), nullptr, nullptr));
		if (!client)
			continue;
		std::string buffer;
		char c;
		while (::read(client.get(), &c, 1) == 1) {
			if (c != '\n') {
				buffer += c;
				continue;
			}
			log << buffer << std::endl;
			std::string reply = "ok";
			if (buffer.rfind("progress ", 0) == 0)
				frame = static_cast<unsigned>(std::atoi(buffer.c_str() + 9));
			else if (buffer == "status")
				reply = "ok frame=" + std::to_string(frame) + " frames=101 paused=0";
			else if (buffer.rfind("load ", 0) == 0 && buffer.find("missing") != std::string::npos)
				reply = "error cannot load";
			reply += "\n";
			(void)!::write(client.get(), reply.data(), reply.size());
			if (buffer.rfind("quit", 0) == 0) {
				::unlink(socket_path.c_str());
				return 0;
			}
			buffer.clear();
		}
	}
}
