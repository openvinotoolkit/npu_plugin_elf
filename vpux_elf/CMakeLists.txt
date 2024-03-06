#
# Copyright (C) 2023 Intel Corporation
# SPDX-License-Identifier: Apache 2.0
#

cmake_minimum_required(VERSION 3.13 FATAL_ERROR)

include( ./cmake/compiler_linker_options.cmake )

set(LIB_NAME npu_elf)

project(${LIB_NAME})

include(cmake/FindFirmwareHeaders.cmake)

include(TestBigEndian)
TEST_BIG_ENDIAN(IS_BIG_ENDIAN)
if(IS_BIG_ENDIAN)
    message(FATAL "ELF Parsing Library supports only little-endian architectures")
endif()

file(GLOB_RECURSE SOURCES CONFIGURE_DEPENDS "*.hpp" "*.cpp")
list(FILTER SOURCES EXCLUDE REGEX "${CMAKE_CURRENT_SOURCE_DIR}/example/.*")

source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR} FILES ${HEADERS} ${SOURCES})

add_library(${LIB_NAME} STATIC ${SOURCES})
add_library(vpux_elf STATIC ${SOURCES})

target_compile_definitions(${LIB_NAME} PUBLIC HOST_BUILD)
target_compile_definitions(vpux_elf PUBLIC HOST_BUILD)

target_include_directories(${LIB_NAME} PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/core/include>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/loader/include>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/hpi_component/include/3720>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/hpi_component/include/common>)

target_include_directories(vpux_elf PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/core/include>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/loader/include>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/hpi_component/include/3720>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/hpi_component/include/common>)

if (DEFINED IE_MAIN_VPUX_PLUGIN_SOURCE_DIR)
    target_include_directories(vpux_elf PUBLIC
        "${IE_MAIN_VPUX_PLUGIN_SOURCE_DIR}/src/vpux_compiler/include/vpux/compiler/dialect/VPU37XX")
endif()

# Set compiler options
target_compile_options(${LIB_NAME} PRIVATE
    $<$<CONFIG:Release>:${UMD_COMPILER_OPTIONS_RELEASE}>
    $<$<CONFIG:Debug>:${UMD_COMPILER_OPTIONS_DEBUG}>
    $<$<CONFIG:RelWithDebInfo>:${UMD_COMPILER_OPTIONS_RELEASE}>)

target_compile_options(vpux_elf PRIVATE
    $<$<CONFIG:Release>:${UMD_COMPILER_OPTIONS_RELEASE}>
    $<$<CONFIG:Debug>:${UMD_COMPILER_OPTIONS_DEBUG}>
    $<$<CONFIG:RelWithDebInfo>:${UMD_COMPILER_OPTIONS_RELEASE}>)

# Set linker options
target_link_options(${LIB_NAME} PRIVATE
    $<$<CONFIG:Release>:${UMD_LINKER_OPTIONS_RELEASE}>
    $<$<CONFIG:Debug>:${UMD_LINKER_OPTIONS_DEBUG}>
    $<$<CONFIG:RelWithDebInfo>:${UMD_LINKER_OPTIONS_RELEASE}>)

# Add external deps
if(EXTERNAL_DEPS)
    target_link_libraries(${LIB_NAME} PUBLIC ${EXTERNAL_DEPS})
    target_link_libraries(vpux_elf PUBLIC ${EXTERNAL_DEPS})
    add_subdirectory(loader)
    if (UNIX)
        # we need for fPIC flags because `npu_elf` is used for dynamic libraries linkage.
        # it seems that this option is set implicitly somewhere due to compile flags overriding...
        # Since `vpux-elf` is expected to be a standalone submodule, we add this option by a purpose
        set_property(TARGET ${LIB_NAME} PROPERTY POSITION_INDEPENDENT_CODE ON)
        set_property(TARGET vpux_elf PROPERTY POSITION_INDEPENDENT_CODE ON)
    endif()
endif(EXTERNAL_DEPS)

target_link_options(vpux_elf PRIVATE
    $<$<CONFIG:Release>:${UMD_LINKER_OPTIONS_RELEASE}>
    $<$<CONFIG:Debug>:${UMD_LINKER_OPTIONS_DEBUG}>
    $<$<CONFIG:RelWithDebInfo>:${UMD_LINKER_OPTIONS_RELEASE}>)

if(NOT BUILD_SHARED_LIBS)
    install(TARGETS ${LIB_NAME} vpux_elf
            RUNTIME DESTINATION ${IE_CPACK_RUNTIME_PATH} COMPONENT ${VPUX_PLUGIN_COMPONENT}
            ARCHIVE DESTINATION ${IE_CPACK_ARCHIVE_PATH} COMPONENT ${VPUX_PLUGIN_COMPONENT}
            LIBRARY DESTINATION ${IE_CPACK_LIBRARY_PATH} COMPONENT ${VPUX_PLUGIN_COMPONENT})
endif()
