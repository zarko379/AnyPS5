if(BUILD_TESTING)
    add_test(NAME agc_driver_minimized_present_drawable COMMAND agc_driver_minimized_present_tests drawable)
    set_tests_properties(agc_driver_minimized_present_drawable PROPERTIES ENVIRONMENT "ANYPS5_NO_SHADER_CACHE=1" SKIP_RETURN_CODE 77 WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}")
    if(WIN32)
        set_tests_properties(agc_driver_minimized_present_drawable PROPERTIES ENVIRONMENT_MODIFICATION
            "PATH=path_list_prepend:$<TARGET_FILE_DIR:libc>")
    endif()
endif()
