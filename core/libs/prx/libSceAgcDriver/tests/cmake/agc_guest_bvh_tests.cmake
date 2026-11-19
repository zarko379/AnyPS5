add_test_executable(agc_guest_bvh_tests EXCLUDE_FROM_ALL tests/GuestBvh.cpp Graphics/src/GuestBvh.cpp)
target_include_directories(agc_guest_bvh_tests PRIVATE ${LIBS_INCLUDE_DIR})
target_link_libraries(agc_guest_bvh_tests PRIVATE libc)
set_target_properties(agc_guest_bvh_tests PROPERTIES CXX_EXTENSIONS OFF)
configure_windows_unwind(agc_guest_bvh_tests)

if(BUILD_TESTING)
    agc_add_default_test(agc_guest_bvh_tests)
endif()
