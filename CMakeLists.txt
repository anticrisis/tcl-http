cmake_minimum_required(VERSION 3.16)

# By default, add the VCPKG toolchain file found in the parent directory of
# this project.
if(NOT DEFINED CMAKE_TOOLCHAIN_FILE)
set(CMAKE_TOOLCHAIN_FILE ${CMAKE_SOURCE_DIR}/../vcpkg/scripts/buildsystems/vcpkg.cmake)
endif()

project(tcl_http
  VERSION 0.1.0
  DESCRIPTION "Tcl extension providing HTTP server"
  LANGUAGES CXX)

set(BASE_INSTALL_DIR ${CMAKE_SOURCE_DIR}/install)

add_library(http_tcl SHARED
    "include/http_tcl/http_tcl.h"
    "act_http/pkgIndex.tcl"
    "src/dllexport.h"
    "src/util.h"
    "src/http_server_sync.cpp"
    "src/http_sync_client.cpp"
    "src/lib.cpp"
    "src/util.cpp"
     )

set_target_properties(http_tcl PROPERTIES 
    OUTPUT_NAME "act_http"
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED YES
    CXX_EXTENSIONS OFF)

add_compile_definitions(USE_TCL_STUBS)
add_compile_definitions(WINVER=0x0A00 _WIN32_WINNT=0x0A00)
find_package(Boost 1.75.0 REQUIRED)

find_path(TCL_INCLUDE_DIR tcl.h REQUIRED)
find_library(TCL_STUB_LIBRARY REQUIRED NAMES tclstub86 PATHS "${TCL_LIBRARY}")

target_include_directories(http_tcl
  PRIVATE
  "${TCL_INCLUDE_DIR}"
  "${CMAKE_SOURCE_DIR}/include"
  ${Boost_INCLUDE_DIRS})

target_link_libraries(http_tcl 
    PRIVATE
    ${TCL_STUB_LIBRARY}
    ${Boost_LIBRARIES}
    )

# because tclStubLib.obj uses link time optimisation
set_target_properties(http_tcl PROPERTIES LINK_FLAGS "/LTCG /INCREMENTAL:no")

# Copy TCL package index
add_custom_target(http_tcl_tcl SOURCES
    "act_http/pkgIndex.tcl"
    COMMAND
    ${CMAKE_COMMAND} -E copy_directory 
        ${CMAKE_CURRENT_SOURCE_DIR}/act_http
        ${CMAKE_CURRENT_BINARY_DIR}/act_http)

# Copy test folder to binary directory
add_custom_target(http_tcl_test SOURCES
    "test/all.tcl"
    "test/http.tcl"
    "test/test-util.tcl"
    COMMAND
    ${CMAKE_COMMAND} -E copy_directory 
        ${CMAKE_CURRENT_SOURCE_DIR}/test
        ${CMAKE_CURRENT_BINARY_DIR}/test)

# Copy examples folder to binary directory
add_custom_target(http_tcl_examples SOURCES
    "examples/userdb.tcl"
    COMMAND
    ${CMAKE_COMMAND} -E copy_directory 
        ${CMAKE_CURRENT_SOURCE_DIR}/examples
        ${CMAKE_CURRENT_BINARY_DIR}/examples)

add_dependencies(http_tcl http_tcl_tcl http_tcl_test http_tcl_examples)

# copy dll into act_http package directory
add_custom_command(TARGET http_tcl POST_BUILD
    COMMAND
    ${CMAKE_COMMAND} -E copy
        $<TARGET_FILE:http_tcl>
        ${CMAKE_CURRENT_BINARY_DIR}/act_http)

# install targets
install(TARGETS http_tcl 
        CONFIGURATIONS Debug
        RUNTIME DESTINATION ${BASE_INSTALL_DIR}/Debug/act_http)

install(TARGETS http_tcl 
        CONFIGURATIONS Release
        RUNTIME DESTINATION ${BASE_INSTALL_DIR}/Release/act_http)

install(TARGETS http_tcl 
        CONFIGURATIONS RelWithDebInfo
        RUNTIME DESTINATION ${BASE_INSTALL_DIR}/RelWithDebInfo/act_http)

# install subdirectories

install(DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/act_http
        CONFIGURATIONS Debug
        DESTINATION ${BASE_INSTALL_DIR}/Debug
        FILES_MATCHING PATTERN "*.tcl")

install(DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/examples
        CONFIGURATIONS Debug
        DESTINATION ${BASE_INSTALL_DIR}/Debug
        FILES_MATCHING PATTERN "*.tcl")

install(DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/act_http
        CONFIGURATIONS Release
        DESTINATION ${BASE_INSTALL_DIR}/Release
        FILES_MATCHING PATTERN "*.tcl")

install(DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/examples
        CONFIGURATIONS Release
        DESTINATION ${BASE_INSTALL_DIR}/Release
        FILES_MATCHING PATTERN "*.tcl")

install(DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/act_http
        CONFIGURATIONS RelWithDebInfo
        DESTINATION ${BASE_INSTALL_DIR}/RelWithDebInfo
        FILES_MATCHING PATTERN "*.tcl")

install(DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/examples
        CONFIGURATIONS RelWithDebInfo
        DESTINATION ${BASE_INSTALL_DIR}/RelWithDebInfo
        FILES_MATCHING PATTERN "*.tcl")
