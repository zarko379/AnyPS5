add_library(libkernel_web SHARED EXCLUDE_FROM_ALL Export.cpp)
target_include_directories(libkernel_web PRIVATE ${LIBS_INCLUDE_DIR})
target_link_libraries(libkernel_web PRIVATE libkernel_sys)
configure_windows_unwind(libkernel_web)

if(BUILD_TESTING)
    add_test_executable(guest_kernel_web_forwards_tests ${LIBS_INCLUDE_DIR}/tests/GuestKernelWebForwards.cpp)
    target_include_directories(guest_kernel_web_forwards_tests PRIVATE ${LIBS_INCLUDE_DIR})
    target_link_libraries(guest_kernel_web_forwards_tests PRIVATE libkernel_web libSceLibcInternal libkernel libc)
    configure_windows_unwind(guest_kernel_web_forwards_tests)
    file(MAKE_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/kernel-web-forwards)
    add_test(NAME guest_kernel_web_forwards COMMAND guest_kernel_web_forwards_tests)
    set_tests_properties(guest_kernel_web_forwards PROPERTIES TIMEOUT 20
        WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/kernel-web-forwards)
    if(WIN32)
        set_tests_properties(guest_kernel_web_forwards PROPERTIES ENVIRONMENT_MODIFICATION
            "PATH=path_list_prepend:$<TARGET_FILE_DIR:libkernel_web>;PATH=path_list_prepend:$<TARGET_FILE_DIR:libkernel_sys>;PATH=path_list_prepend:$<TARGET_FILE_DIR:libSceLibcInternal>")
    endif()
endif()
