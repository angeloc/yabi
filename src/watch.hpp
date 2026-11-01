// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include "unique_fd.hpp"

#include <map>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace yabi {

// The S??* scripts of the given init directories in run order: each
// directory sorted by name, the directories in the order given.
std::vector<std::string> boot_scripts(const std::vector<std::string> &dirs);

// Percentage of the OpenRC services of the boot runlevels already started,
// nothing when OpenRC is not running under root.
std::optional<int> openrc_progress(const std::string &root = "");

// A source of boot progress: fd() becomes readable on events, progress()
// reads them.
class Watcher {
public:
	virtual ~Watcher() = default;
	virtual int fd() const = 0;

	// Reads the pending events; returns the boot progress in percent,
	// nothing while it is unknown.
	virtual std::optional<int> progress() = 0;
};

// Follows the boot through inotify, without polling: rc opening an init
// script, or OpenRC marking a service started, makes fd() readable.
class BootWatcher final : public Watcher {
public:
	// Throws std::system_error when inotify is not available.
	explicit BootWatcher(const std::vector<std::string> &dirs, const std::string &root = "");

	int fd() const override { return fd_.get(); }
	std::optional<int> progress() override;

private:
	UniqueFd fd_;
	std::string root_;
	bool openrc_ = false;
	std::size_t scripts_ = 0;
	// (watch descriptor, file name) of each script's real file -> index
	std::map<std::pair<int, std::string>, std::size_t> index_;
	std::optional<std::size_t> latest_;
};

} // namespace yabi
