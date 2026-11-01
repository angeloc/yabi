// SPDX-License-Identifier: GPL-2.0-or-later
// SystemdWatcher on the user's systemd instance: a transient unit's job
// ending must wake it. Skipped (77) without a user instance.
#include "check.hpp"
#include "systemd_watch.hpp"

#include <cstdlib>
#include <poll.h>

using namespace yabi;

int main()
{
	std::optional<SystemdWatcher> watcher;
	try {
		watcher.emplace(SystemdWatcher::Bus::user);
	} catch (const std::exception &e) {
		std::fprintf(stderr, "skipped: %s\n", e.what());
		return 77;
	}

	const auto now = watcher->progress();
	CHECK(now && *now >= 0 && *now <= 100);
	CHECK(!watcher->progress());

	if (std::system("systemd-run --user --quiet --collect true") != 0) {
		std::fprintf(stderr, "skipped: systemd-run --user failed\n");
		return 77;
	}
	bool woke = false;
	for (int i = 0; i < 20 && !woke; i++) {
		pollfd p{watcher->fd(), POLLIN, 0};
		if (::poll(&p, 1, 100) > 0)
			woke = watcher->progress().has_value();
	}
	CHECK(woke);
	return check_result();
}
