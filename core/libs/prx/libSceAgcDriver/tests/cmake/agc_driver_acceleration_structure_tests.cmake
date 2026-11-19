set(agcRayQueryProbeShaderSource ${CMAKE_CURRENT_SOURCE_DIR}/tests/shaders/RayQueryProbe.comp)
set(agcRayQueryProbeShaderSpv ${agcTestShaderGeneratedRoot}/RayQueryProbe.spv)
set(agcRayQueryProbeShaderHeader ${agcTestShaderGeneratedRoot}/RayQueryProbe_spv.h)
add_custom_command(
        OUTPUT "${agcRayQueryProbeShaderHeader}"
        COMMAND ${CMAKE_COMMAND} -E make_directory "${agcTestShaderGeneratedRoot}"
        COMMAND "$<TARGET_FILE:glslang-standalone>" -V --target-env spirv1.4 -o "${agcRayQueryProbeShaderSpv}" "${agcRayQueryProbeShaderSource}"
        COMMAND ${CMAKE_COMMAND} -DINPUT=${agcRayQueryProbeShaderSpv} -DOUTPUT=${agcRayQueryProbeShaderHeader}
        -DSYMBOL=RAY_QUERY_PROBE_SPV -P "${CMAKE_CURRENT_SOURCE_DIR}/embed_spirv.cmake"
        DEPENDS "${agcRayQueryProbeShaderSource}" glslang-standalone
        VERBATIM
)

add_test_executable(agc_driver_acceleration_structure_tests EXCLUDE_FROM_ALL
        tests/AccelerationStructure.cpp
        ${agcRayQueryProbeShaderHeader}
)

target_include_directories(agc_driver_acceleration_structure_tests PRIVATE
        ${LIBS_INCLUDE_DIR}
        ${agcTestShaderGeneratedRoot}
        ${CMAKE_SOURCE_DIR}/core/shader/recompiler
        ${ANYPS5_VULKAN_HEADERS_INCLUDE_DIR}
)

target_link_libraries(agc_driver_acceleration_structure_tests PRIVATE
        libSceAgcDriver
        SDL2-static
        libc
)

set_target_properties(agc_driver_acceleration_structure_tests PROPERTIES CXX_EXTENSIONS OFF)
configure_windows_unwind(agc_driver_acceleration_structure_tests)

if(BUILD_TESTING)
    set_target_properties(agc_driver_acceleration_structure_tests PROPERTIES EXCLUDE_FROM_ALL OFF)
    add_test(NAME agc_driver_acceleration_structure COMMAND agc_driver_acceleration_structure_tests)
    set_tests_properties(agc_driver_acceleration_structure PROPERTIES ENVIRONMENT "ANYPS5_NO_SHADER_CACHE=1;APS5_NATIVE_RT=1" SKIP_RETURN_CODE 77 TIMEOUT 60 WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}")
    if(WIN32)
        set_tests_properties(agc_driver_acceleration_structure PROPERTIES ENVIRONMENT_MODIFICATION
                "PATH=path_list_prepend:$<TARGET_FILE_DIR:libc>")
    endif()
endif()
