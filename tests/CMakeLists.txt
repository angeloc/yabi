add_executable(unit_test unit_test.cpp)
target_link_libraries(unit_test PRIVATE yabi_core)
add_test(NAME unit COMMAND unit_test)

# The CLI against a stand-in for fbv speaking its control protocol
add_executable(fake_fbv fake_fbv.cpp)
target_link_libraries(fake_fbv PRIVATE yabi_core)
add_executable(cli_test cli_test.cpp)
target_link_libraries(cli_test PRIVATE yabi_core)
add_test(NAME cli COMMAND cli_test $<TARGET_FILE:yabi> $<TARGET_FILE:fake_fbv> ${PROJECT_SOURCE_DIR}/themes)
# The test starts yabi and fake_fbv itself; under qemu-user binfmt they
# find the target libraries through QEMU_LD_PREFIX.
if(CMAKE_CROSSCOMPILING AND CMAKE_SYSROOT)
	set_tests_properties(cli PROPERTIES ENVIRONMENT "QEMU_LD_PREFIX=${CMAKE_SYSROOT}")
endif()

if(YABI_SYSTEMD)
	add_executable(systemd_test systemd_test.cpp)
	target_link_libraries(systemd_test PRIVATE yabi_core)
	add_test(NAME systemd COMMAND systemd_test)
	set_tests_properties(systemd PROPERTIES SKIP_RETURN_CODE 77)
endif()

# With a real fbv (fbv-ng with --control), on a fake framebuffer
set(YABI_TEST_FBV "" CACHE FILEPATH "fbv to run the end-to-end test with")
if(YABI_TEST_FBV AND NOT CMAKE_CROSSCOMPILING)
	add_library(fakefb MODULE fakefb.cpp)
	add_test(NAME fbv COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/fbv_test.sh
		$<TARGET_FILE:yabi> ${YABI_TEST_FBV} $<TARGET_FILE:fakefb> ${PROJECT_SOURCE_DIR}/themes)
endif()
