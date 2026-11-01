// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include <stdexcept>
#include <string>

namespace yabi {

class ThemeError : public std::runtime_error {
public:
	using std::runtime_error::runtime_error;
};

// A theme is a directory holding theme.conf and its animation:
//
//   # comment
//   animation = boot.gif
//   mode = progress      # progress: hold a frame driven by "yabi progress"
//                        # loop: play the animation freely
//   scale = fit          # none, shrink or fit
struct Theme {
	enum class Mode { progress, loop };
	enum class Scale { none, shrink, fit };

	std::string animation;  // absolute path
	Mode mode = Mode::progress;
	Scale scale = Scale::none;
};

// Loads theme.conf from dir; throws ThemeError naming the file and line.
Theme load_theme(const std::string &dir);

// A name containing a slash is a directory, otherwise it is looked up
// in themes_dir.
std::string theme_directory(const std::string &name, const std::string &themes_dir);

} // namespace yabi
