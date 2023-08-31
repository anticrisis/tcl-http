cmake_minimum_required(VERSION 3.16)

if(DEFINED ENV{VCPKG_DEFAULT_TRIPLET} AND NOT DEFINED VCPKG_TARGET_TRIPLET)
  set(VCPKG_TARGET_TRIPLET "$ENV{VCPKG_DEFAULT_TRIPLET}" CACHE STRING "")
endif()

if(DEFINED ENV{VCPKG_ROOT} AND NOT DEFINED CMAKE_TOOLCHAIN_FILE)
  set(CMAKE_TOOLCHAIN_FILE "$ENV{VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake"
      CACHE STRING "")
endif()

project(tcl_http
  VERSION 0.1
  DESCRIPTION "Tcl extension providing HTTP server"
  LANGUAGES CXX)

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

configure_file(src/version.h.in version.h)
file(GENERATE OUTPUT libname.txt CONTENT $<TARGET_FILE_NAME:http_tcl>)

# CMake knows how to find TCL
find_package(TCL)
find_package(TclStub)
add_compile_definitions(USE_TCL_STUBS)
find_package(Boost 1.75.0 REQUIRED)

if(MSVC)
    add_compile_definitions(WINVER=0x0A00 _WIN32_WINNT=0x0A00)
endif()

find_path(TCL_INCLUDE_DIR tcl.h REQUIRED
    HINTS
    ${CMAKE_SOURCE_DIR}/../tcl/generic
    ${CMAKE_SOURCE_DIR}/../tcl86/generic
    ${CMAKE_SOURCE_DIR}/../tcl87/generic
    /usr/local/tcl86/include
    /usr/local/tcl87/include
    /usr/include/tcl8.6
    /usr/include/tcl8.7)

target_include_directories(http_tcl
  PRIVATE
  "${CMAKE_CURRENT_BINARY_DIR}"
  "${TCL_INCLUDE_PATH}"
  "${CMAKE_SOURCE_DIR}/include"
  ${Boost_INCLUDE_DIRS})

target_link_libraries(http_tcl
    PRIVATE
    ${TCL_STUB_LIBRARY}
    ${Boost_LIBRARIES}
    )

if(MSVC)
    # because tclStubLib.obj uses link time optimisation
    set_target_properties(http_tcl PROPERTIES LINK_FLAGS "/LTCG /INCREMENTAL:no")
endif()

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
