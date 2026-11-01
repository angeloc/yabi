// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include "watch.hpp"

#include <systemd/sd-bus.h>

namespace yabi {

// True when systemd runs as PID 1.
bool systemd_booted();

// Follows the boot through systemd itself: it reports every finished job
// and the end of the startup, and keeps the boot progress, from the jobs
// done and queued, in the manager's Progress property.
class SystemdWatcher final : public Watcher {
public:
	enum class Bus { system, user };

	// The system manager is reached directly through its private socket,
	// so this works before D-Bus is up. Throws std::runtime_error.
	explicit SystemdWatcher(Bus bus = Bus::system);
	~SystemdWatcher() override;
	SystemdWatcher(const SystemdWatcher &) = delete;
	SystemdWatcher &operator=(const SystemdWatcher &) = delete;

	int fd() const override;
	std::optional<int> progress() override;

private:
	void release();
	static int on_job_removed(sd_bus_message *m, void *self, sd_bus_error *error);
	static int on_startup_finished(sd_bus_message *m, void *self, sd_bus_error *error);

	sd_bus *bus_ = nullptr;
	sd_bus_slot *jobs_ = nullptr;
	sd_bus_slot *finished_slot_ = nullptr;
	bool changed_ = true;
	bool finished_ = false;
};

} // namespace yabi
