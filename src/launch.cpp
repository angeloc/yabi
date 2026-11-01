// SPDX-License-Identifier: GPL-2.0-or-later
#include "launch.hpp"
#include "unique_fd.hpp"

#include <cerrno>
#include <fcntl.h>
#include <stdexcept>
#include <system_error>
#include <sys/wait.h>
#include <unistd.h>

namespace yabi {

pid_t launch(const std::vector<std::string> &args)
{
	if (args.empty())
		throw std::invalid_argument("nothing to launch");

	// Everything the child needs is prepared before fork.
	std::vector<char *> argv;
	for (const auto &a : args)
		argv.push_back(const_cast<char *>(a.c_str()));
	argv.push_back(nullptr);

	// The child reports a failed exec through this close-on-exec pipe.
	int fds[2];
	if (::pipe2(fds, O_CLOEXEC) < 0)
		throw std::system_error(errno, std::generic_category(), "pipe");
	UniqueFd rd(fds[0]), wr(fds[1]);

	const pid_t pid = ::fork();
	if (pid < 0)
		throw std::system_error(errno, std::generic_category(), "fork");
	if (pid == 0) {
		// Own session, so it survives the shell or init script that
		// started it; only async-signal-safe calls from here.
		::setsid();
		const int null = ::open("/dev/null", O_RDWR);
		if (null >= 0) {
			::dup2(null, STDIN_FILENO);
			::dup2(null, STDOUT_FILENO);
			::dup2(null, STDERR_FILENO);
		}
		::execvp(argv[0], argv.data());
		const int err = errno;
		(void)!::write(wr.get(), &err, sizeof(err));
		::_exit(127);
	}

	wr.reset();
	int err = 0;
	ssize_t n;
	while ((n = ::read(rd.get(), &err, sizeof(err))) < 0 && errno == EINTR)
		;
	if (n == sizeof(err)) {
		::waitpid(pid, nullptr, 0);
		throw std::system_error(err, std::generic_category(), "cannot start " + args[0]);
	}
	return pid;
}

bool running(pid_t pid)
{
	int status;
	return ::waitpid(pid, &status, WNOHANG) == 0;
}

} // namespace yabi
