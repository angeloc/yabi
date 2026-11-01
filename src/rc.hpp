// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include <functional>
#include <string>
#include <vector>

namespace yabi {

// The S??* scripts of an init.d directory, in run order.
std::vector<std::string> rc_scripts(const std::string &dir);

// Runs each script with "start", calling progress with the percentage done
// before and after. Returns the number of scripts that failed.
int run_rc(const std::vector<std::string> &scripts, const std::function<void(int)> &progress);

} // namespace yabi
