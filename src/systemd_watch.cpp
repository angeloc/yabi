// SPDX-License-Identifier: GPL-2.0-or-later
#include "systemd_watch.hpp"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <stdexcept>
#include <string>
#include <systemd/sd-bus.h>
#include <systemd/sd-daemon.h>

namespace yabi {

namespace {

constexpr const char *service = "org.freedesktop.systemd1";
constexpr const char *object = "/org/freedesktop/systemd1";
constexpr const char *manager = "org.freedesktop.systemd1.Manager";

[[noreturn]] void fail(const char *what, int r)
{
	throw std::runtime_error(std::string(what) + ": " + std::strerror(-r));
}

// PID 1 listens on this socket without needing a bus daemon.
int open_private(sd_bus **bus)
{
	int r = sd_bus_new(bus);
	if (r < 0)
		return r;
	r = sd_bus_set_address(*bus, "unix:path=/run/systemd/private");
	if (r >= 0)
		r = sd_bus_start(*bus);
	if (r < 0)
		*bus = sd_bus_unref(*bus);
	return r;
}

} // namespace

bool systemd_booted()
{
	return sd_booted() > 0;
}

SystemdWatcher::SystemdWatcher(Bus bus)
{
	int r = bus == Bus::user ? sd_bus_open_user(&bus_) : open_private(&bus_);
	if (r < 0 && bus == Bus::system)
		r = sd_bus_open_system(&bus_);
	if (r < 0)
		fail("cannot reach systemd", r);

	// The sender is left out: a private connection carries none.
	r = sd_bus_match_signal(bus_, &jobs_, nullptr, object, manager, "JobRemoved", on_job_removed, this);
	if (r >= 0)
		r = sd_bus_match_signal(bus_, &finished_slot_, nullptr, object, manager, "StartupFinished",
					on_startup_finished, this);
	if (r < 0) {
		release();
		fail("cannot watch systemd", r);
	}

	// The manager sends its signals only to subscribed clients.
	sd_bus_error error = SD_BUS_ERROR_NULL;
	r = sd_bus_call_method(bus_, service, object, manager, "Subscribe", &error, nullptr, "");
	sd_bus_error_free(&error);
	if (r < 0) {
		release();
		fail("cannot subscribe to systemd", r);
	}
}

SystemdWatcher::~SystemdWatcher()
{
	release();
}

void SystemdWatcher::release()
{
	jobs_ = sd_bus_slot_unref(jobs_);
	finished_slot_ = sd_bus_slot_unref(finished_slot_);
	bus_ = sd_bus_flush_close_unref(bus_);
}

int SystemdWatcher::on_job_removed(sd_bus_message *, void *self, sd_bus_error *)
{
	static_cast<SystemdWatcher *>(self)->changed_ = true;
	return 0;
}

int SystemdWatcher::on_startup_finished(sd_bus_message *, void *self, sd_bus_error *)
{
	static_cast<SystemdWatcher *>(self)->finished_ = true;
	return 0;
}

int SystemdWatcher::fd() const
{
	return sd_bus_get_fd(bus_);
}

std::optional<int> SystemdWatcher::progress()
{
	while (sd_bus_process(bus_, nullptr) > 0)
		;
	if (finished_)
		return 100;
	if (!changed_)
		return std::nullopt;
	changed_ = false;

	double value = 0;
	sd_bus_error error = SD_BUS_ERROR_NULL;
	const int r = sd_bus_get_property_trivial(bus_, service, object, manager, "Progress", &error, 'd', &value);
	sd_bus_error_free(&error);
	if (r < 0 || !std::isfinite(value))
		return std::nullopt;
	return static_cast<int>(std::clamp(value, 0.0, 1.0) * 100);
}

} // namespace yabi
