// SPDX-License-Identifier: GPL-2.0-or-later
#include "client.hpp"

#include <algorithm>
#include <cerrno>
#include <cstring>
#include <poll.h>
#include <stdexcept>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

namespace yabi {

namespace {

constexpr std::size_t max_reply = 4096;

using Clock = std::chrono::steady_clock;

// Waits for input until deadline; false on timeout.
bool readable(int fd, Clock::time_point deadline)
{
	for (;;) {
		const auto left = std::chrono::ceil<std::chrono::milliseconds>(deadline - Clock::now()).count();
		pollfd p{fd, POLLIN, 0};
		const int rc = ::poll(&p, 1, static_cast<int>(std::clamp<long long>(left, 0, 60000)));
		if (rc > 0)
			return true;
		if (rc == 0 && left <= 0)
			return false;
		if (rc < 0 && errno != EINTR)
			throw std::runtime_error(std::string("poll: ") + std::strerror(errno));
	}
}

} // namespace

std::optional<Client> Client::connect(const std::string &path)
{
	sockaddr_un addr{};
	addr.sun_family = AF_UNIX;
	if (path.empty() || path.size() >= sizeof(addr.sun_path))
		throw std::runtime_error("invalid socket path: " + path);
	std::memcpy(addr.sun_path, path.c_str(), path.size() + 1);

	UniqueFd fd(::socket(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0));
	if (!fd)
		throw std::runtime_error(std::string("socket: ") + std::strerror(errno));
	if (::connect(fd.get(), reinterpret_cast<const sockaddr *>(&addr), sizeof(addr)) < 0)
		return std::nullopt;
	return Client(std::move(fd));
}

Reply Client::request(const std::string &command, std::chrono::milliseconds timeout)
{
	if (command.find('\n') != std::string::npos)
		throw std::runtime_error("command contains a newline");
	const std::string line = command + "\n";
	if (::send(fd_.get(), line.data(), line.size(), MSG_NOSIGNAL) != static_cast<ssize_t>(line.size()))
		throw std::runtime_error("cannot send to fbv");

	const auto deadline = Clock::now() + timeout;
	std::size_t eol;
	while ((eol = buffer_.find('\n')) == std::string::npos) {
		if (buffer_.size() > max_reply)
			throw std::runtime_error("reply too long");
		if (!readable(fd_.get(), deadline))
			throw std::runtime_error("fbv did not answer");
		char chunk[512];
		const ssize_t n = ::read(fd_.get(), chunk, sizeof(chunk));
		if (n < 0 && errno == EINTR)
			continue;
		if (n <= 0)
			throw std::runtime_error("fbv closed the connection");
		buffer_.append(chunk, static_cast<std::size_t>(n));
	}
	std::string reply = buffer_.substr(0, eol);
	buffer_.erase(0, eol + 1);

	if (reply == "ok")
		return {true, {}};
	if (reply.rfind("ok ", 0) == 0)
		return {true, reply.substr(3)};
	if (reply.rfind("error ", 0) == 0)
		return {false, reply.substr(6)};
	throw std::runtime_error("unexpected reply: " + reply);
}

bool Client::wait_closed(std::chrono::milliseconds timeout)
{
	const auto deadline = Clock::now() + timeout;
	for (;;) {
		if (!readable(fd_.get(), deadline))
			return false;
		char chunk[512];
		const ssize_t n = ::read(fd_.get(), chunk, sizeof(chunk));
		if (n == 0)
			return true;
		if (n < 0 && errno != EINTR && errno != EAGAIN)
			return true;
	}
}

} // namespace yabi
