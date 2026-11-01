// SPDX-License-Identifier: GPL-2.0-or-later
#include "theme.hpp"

#include <fstream>
#include <limits.h>
#include <stdlib.h>
#include <unistd.h>

namespace yabi {

namespace {

constexpr std::size_t max_line = 1024;

std::string trim(const std::string &s)
{
	const auto first = s.find_first_not_of(" \t\r");
	if (first == std::string::npos)
		return {};
	return s.substr(first, s.find_last_not_of(" \t\r") - first + 1);
}

} // namespace

std::string theme_directory(const std::string &name, const std::string &themes_dir)
{
	if (name.empty())
		throw ThemeError("empty theme name");
	if (name.find('/') != std::string::npos)
		return name;
	if (name == "." || name == "..")
		throw ThemeError("invalid theme name " + name);
	return themes_dir + "/" + name;
}

Theme load_theme(const std::string &dir)
{
	const std::string path = dir + "/theme.conf";
	std::ifstream in(path);
	if (!in)
		throw ThemeError(path + ": cannot open");

	Theme theme;
	std::string line;
	for (unsigned number = 1; std::getline(in, line); number++) {
		auto fail = [&](const std::string &what) -> ThemeError {
			return ThemeError(path + ":" + std::to_string(number) + ": " + what);
		};
		if (line.size() > max_line)
			throw fail("line too long");
		const auto hash = line.find('#');
		if (hash != std::string::npos)
			line.erase(hash);
		line = trim(line);
		if (line.empty())
			continue;

		const auto eq = line.find('=');
		if (eq == std::string::npos)
			throw fail("expected key = value");
		const std::string key = trim(line.substr(0, eq));
		const std::string value = trim(line.substr(eq + 1));
		if (value.empty())
			throw fail("missing value for " + key);

		if (key == "animation") {
			theme.animation = value.front() == '/' ? value : dir + "/" + value;
		} else if (key == "mode") {
			if (value == "progress")
				theme.mode = Theme::Mode::progress;
			else if (value == "loop")
				theme.mode = Theme::Mode::loop;
			else
				throw fail("mode must be progress or loop");
		} else if (key == "scale") {
			if (value == "none")
				theme.scale = Theme::Scale::none;
			else if (value == "shrink")
				theme.scale = Theme::Scale::shrink;
			else if (value == "fit")
				theme.scale = Theme::Scale::fit;
			else
				throw fail("scale must be none, shrink or fit");
		} else {
			throw fail("unknown key " + key);
		}
	}
	if (in.bad())
		throw ThemeError(path + ": read error");
	if (theme.animation.empty())
		throw ThemeError(path + ": no animation");

	// fbv runs from another directory, give it an absolute path.
	char resolved[PATH_MAX];
	if (!::realpath(theme.animation.c_str(), resolved))
		throw ThemeError(path + ": animation " + theme.animation + " not found");
	theme.animation = resolved;
	return theme;
}

} // namespace yabi
