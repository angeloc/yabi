cmake_minimum_required(VERSION 3.16)

project(yabi VERSION 0.1.0 DESCRIPTION "Yet another bootsplash implementation" LANGUAGES CXX)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
	set(CMAKE_BUILD_TYPE MinSizeRel CACHE STRING "Build type" FORCE)
endif()

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(YABI_DEFAULT_SOCKET "/run/yabi.sock" CACHE STRING "Control socket used when none is given")
set(YABI_THEME_DIR "${CMAKE_INSTALL_PREFIX}/share/yabi/themes" CACHE STRING "Directory holding the themes")
option(YABI_HARDENING "Compiler and linker hardening" ON)
option(YABI_SANITIZE "Build with AddressSanitizer and UndefinedBehaviorSanitizer" OFF)

include(CheckCXXCompilerFlag)
include(CTest)
include(GNUInstallDirs)
if(NOT CMAKE_VERSION VERSION_LESS 3.18)
	include(CheckLinkerFlag)
endif()

# Flags shared by every target. Each one is probed, so any GCC or Clang
# toolchain, native or cross, gets what it supports.
add_library(yabi_flags INTERFACE)

function(yabi_add_flags)
	foreach(flag IN LISTS ARGN)
		string(MAKE_C_IDENTIFIER "YABI_HAVE${flag}" var)
		check_cxx_compiler_flag(${flag} ${var})
		if(${var})
			target_compile_options(yabi_flags INTERFACE ${flag})
		endif()
	endforeach()
endfunction()

function(yabi_add_link_flags)
	if(NOT COMMAND check_linker_flag)
		return()
	endif()
	foreach(flag IN LISTS ARGN)
		string(MAKE_C_IDENTIFIER "YABI_HAVE_LD${flag}" var)
		check_linker_flag(CXX "${flag}" ${var})
		if(${var})
			target_link_options(yabi_flags INTERFACE ${flag})
		endif()
	endforeach()
endfunction()

yabi_add_flags(-Wall -Wextra -Wpedantic -Wshadow -Wnon-virtual-dtor -Wcast-align
	-Wformat=2 -Wnull-dereference -Wimplicit-fallthrough)

if(YABI_HARDENING)
	include(CheckPIESupported)
	check_pie_supported(LANGUAGES CXX)
	set(CMAKE_POSITION_INDEPENDENT_CODE ON)
	target_compile_definitions(yabi_flags INTERFACE _GLIBCXX_ASSERTIONS)
	if(NOT YABI_SANITIZE)
		target_compile_options(yabi_flags INTERFACE
			$<$<NOT:$<CONFIG:Debug>>:-U_FORTIFY_SOURCE -D_FORTIFY_SOURCE=2>)
	endif()
	yabi_add_flags(-fstack-protector-strong -fstack-clash-protection -fcf-protection)
	yabi_add_link_flags(LINKER:-z,relro LINKER:-z,now LINKER:-z,noexecstack)
endif()

if(YABI_SANITIZE)
	target_compile_options(yabi_flags INTERFACE
		-fsanitize=address,undefined -fno-sanitize-recover=all -fno-omit-frame-pointer)
	target_link_options(yabi_flags INTERFACE -fsanitize=address,undefined)
endif()

# Everything but main, shared with the tests
add_library(yabi_core STATIC
	src/client.cpp
	src/launch.cpp
	src/rc.cpp
	src/theme.cpp)
target_include_directories(yabi_core PUBLIC src)
target_link_libraries(yabi_core PUBLIC yabi_flags)

add_executable(yabi src/main.cpp)
target_link_libraries(yabi PRIVATE yabi_core)
target_compile_definitions(yabi PRIVATE
	YABI_VERSION="${PROJECT_VERSION}"
	YABI_DEFAULT_SOCKET="${YABI_DEFAULT_SOCKET}"
	YABI_THEME_DIR="${YABI_THEME_DIR}")

install(TARGETS yabi RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
install(FILES yabi.1 DESTINATION ${CMAKE_INSTALL_MANDIR}/man1)
