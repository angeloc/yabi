// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include "unique_fd.hpp"

#include <chrono>
#include <optional>
#include <string>

namespace yabi {

struct Reply {
	bool ok;
	std::string text;  // after "ok " or "error "
};

// One connection to fbv's control socket.
class Client {
public:
	// Returns nothing when nobody serves the socket.
	static std::optional<Client> connect(const std::string &path);

	// Sends one command and waits for its reply line; throws
	// std::runtime_error on timeout or a broken connection.
	Reply request(const std::string &command, std::chrono::milliseconds timeout);

	// Waits until fbv closes the connection, i.e. has exited.
	bool wait_closed(std::chrono::milliseconds timeout);

	// Readable when fbv closes the connection; replies are read by request().
	int fd() const { return fd_.get(); }

private:
	explicit Client(UniqueFd fd) : fd_(std::move(fd)) {}

	UniqueFd fd_;
	std::string buffer_;
};

} // namespace yabi
