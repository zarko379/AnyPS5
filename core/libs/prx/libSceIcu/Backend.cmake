set(icuSourceDir ${CMAKE_SOURCE_DIR}/3rdparty/icu68/icu4c/source)
if(NOT Python3_EXECUTABLE)
    find_package(Python3 REQUIRED COMPONENTS Interpreter)
endif()
set(icuDataName icudt68l)

function(icu_backend_sources variable directory)
    file(STRINGS ${icuSourceDir}/${directory}/sources.txt names)
    set(sources "")
    foreach(name IN LISTS names)
        string(STRIP "${name}" name)
        if(name)
            list(APPEND sources ${icuSourceDir}/${directory}/${name})
        endif()
    endforeach()
    set(${variable} ${sources} PARENT_SCOPE)
endfunction()

icu_backend_sources(icuCommonSources common)
icu_backend_sources(icuI18nSources i18n)
icu_backend_sources(icuStubDataSources stubdata)
icu_backend_sources(icuToolUtilSources tools/toolutil)

add_library(icu_common_backend STATIC EXCLUDE_FROM_ALL ${icuCommonSources})
target_include_directories(icu_common_backend PUBLIC ${icuSourceDir}/common)
target_compile_definitions(icu_common_backend PRIVATE U_COMMON_IMPLEMENTATION UCONFIG_USE_WINDOWS_LCID_MAPPING_API=0 PUBLIC U_STATIC_IMPLEMENTATION U_ATTRIBUTE_DEPRECATED= UCONFIG_NO_FILE_IO=1)

add_library(icu_i18n_backend STATIC EXCLUDE_FROM_ALL ${icuI18nSources})
target_include_directories(icu_i18n_backend PUBLIC ${icuSourceDir}/i18n)
target_compile_definitions(icu_i18n_backend PRIVATE U_I18N_IMPLEMENTATION)
target_link_libraries(icu_i18n_backend PUBLIC icu_common_backend)

foreach(icuBackendTarget icu_common_backend icu_i18n_backend)
    set_target_properties(${icuBackendTarget} PROPERTIES
        POSITION_INDEPENDENT_CODE ON
        CXX_VISIBILITY_PRESET hidden
        VISIBILITY_INLINES_HIDDEN ON)
endforeach()

add_library(icu_common_tool STATIC EXCLUDE_FROM_ALL ${icuCommonSources})
target_include_directories(icu_common_tool PUBLIC ${icuSourceDir}/common)
target_compile_definitions(icu_common_tool PRIVATE U_COMMON_IMPLEMENTATION PUBLIC U_STATIC_IMPLEMENTATION U_ATTRIBUTE_DEPRECATED=)
add_library(icu_i18n_tool STATIC EXCLUDE_FROM_ALL ${icuI18nSources})
target_include_directories(icu_i18n_tool PUBLIC ${icuSourceDir}/i18n)
target_compile_definitions(icu_i18n_tool PRIVATE U_I18N_IMPLEMENTATION)
target_link_libraries(icu_i18n_tool PUBLIC icu_common_tool)
set(icuToolLibraries icu_i18n_tool icu_common_tool)

add_library(icu_stubdata_tool STATIC EXCLUDE_FROM_ALL ${icuStubDataSources})
target_include_directories(icu_stubdata_tool PRIVATE ${icuSourceDir}/common)
target_compile_definitions(icu_stubdata_tool PRIVATE U_COMMON_IMPLEMENTATION PUBLIC U_STATIC_IMPLEMENTATION)

add_library(icu_toolutil_tool STATIC EXCLUDE_FROM_ALL ${icuToolUtilSources})
target_include_directories(icu_toolutil_tool PUBLIC ${icuSourceDir}/tools/toolutil)
target_compile_definitions(icu_toolutil_tool PRIVATE U_TOOLUTIL_IMPLEMENTATION)
target_link_libraries(icu_toolutil_tool PUBLIC ${icuToolLibraries})

if(MINGW)
    foreach(icuToolLibrary icu_common_tool icu_i18n_tool icu_stubdata_tool icu_toolutil_tool)
        target_compile_options(${icuToolLibrary} PRIVATE -fasynchronous-unwind-tables)
    endforeach()
endif()

set(icuToolDir ${CMAKE_CURRENT_BINARY_DIR}/icu-tools)
set(icuTools genrb genbrk gencnval gencfu gendict gensprep makeconv icupkg gennorm2)
set(icuToolTargets "")
foreach(icuTool IN LISTS icuTools)
    icu_backend_sources(icuToolSources tools/${icuTool})
    add_executable(icu_tool_${icuTool} EXCLUDE_FROM_ALL ${icuToolSources})
    target_link_libraries(icu_tool_${icuTool} PRIVATE icu_toolutil_tool ${icuToolLibraries} icu_stubdata_tool)
    configure_host_test_executable(icu_tool_${icuTool})
    if(WIN32)
        set_target_properties(icu_tool_${icuTool} PROPERTIES OUTPUT_NAME ${icuTool}
            RUNTIME_OUTPUT_DIRECTORY ${icuToolDir}/${icuTool}/Release
            RUNTIME_OUTPUT_DIRECTORY_RELEASE ${icuToolDir}/${icuTool}/Release)
    else()
        set_target_properties(icu_tool_${icuTool} PROPERTIES OUTPUT_NAME ${icuTool}
            RUNTIME_OUTPUT_DIRECTORY ${icuToolDir}
            RUNTIME_OUTPUT_DIRECTORY_RELEASE ${icuToolDir})
    endif()
    list(APPEND icuToolTargets icu_tool_${icuTool})
endforeach()

if(WIN32)
    set(icuDataMode windows-exec)
    set(icuPackageTool ${icuToolDir}/icupkg/Release/icupkg${CMAKE_EXECUTABLE_SUFFIX})
else()
    set(icuDataMode unix-exec)
    set(icuPackageTool ${icuToolDir}/icupkg)
endif()

set(icuDataWorkDir ${CMAKE_CURRENT_BINARY_DIR}/icu-data)
set(icuDataFile ${icuDataWorkDir}/${icuDataName}.dat)
add_custom_command(
    OUTPUT ${icuDataFile}
    COMMAND ${CMAKE_COMMAND} -E rm -rf ${icuDataWorkDir}/out
    COMMAND ${CMAKE_COMMAND} -E env PYTHONPATH=${icuSourceDir}/python
            ${Python3_EXECUTABLE} -m icutools.databuilder --mode ${icuDataMode}
            --src_dir ${icuSourceDir}/data --tool_dir ${icuToolDir} --tool_cfg Release
            --out_dir ${icuDataWorkDir}/out/build/${icuDataName} --tmp_dir ${icuDataWorkDir}/out/tmp
    COMMAND ${icuPackageTool} -tl -s ${icuDataWorkDir}/out/build/${icuDataName}
            -a ${icuDataWorkDir}/out/tmp/icudata.lst new ${icuDataFile}
    DEPENDS ${icuToolTargets}
    WORKING_DIRECTORY ${icuSourceDir}
    COMMENT "Generating ICU 68.2 data ${icuDataName}.dat"
    VERBATIM)
add_custom_target(icu_backend_data DEPENDS ${icuDataFile})

install(FILES ${CMAKE_SOURCE_DIR}/3rdparty/icu68/icu4c/LICENSE DESTINATION licenses/icu68)
