// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include <chrono>
#include <string>
#include <sys/types.h>
#include <vector>

namespace yabi {

// Starts a program detached from yabi: own session, standard streams on
// /dev/null. Throws std::system_error when it cannot be started.
pid_t launch(const std::vector<std::string> &args);

// True while pid runs; reaps it when it exited.
bool running(pid_t pid);

} // namespace yabi
