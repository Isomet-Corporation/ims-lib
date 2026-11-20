# Unit tests (GoogleTest, run with ctest).  Enabled with -DIMSLIB_BUILD_TESTS=ON.
#
# The tests need the library internals (CM_Common, Message, report parsing), which the shared
# library does not export.  So the library sources are compiled a second time into a static
# library, ims_testable, using the same include directories, definitions and dependencies as
# the shipped target.  The shipped library is not changed.

include(GoogleTest)

# ---------------------------------------------------------------------------
# GoogleTest: use an installed copy if there is one (Conan, vcpkg, system),
# otherwise download a pinned release.
# ---------------------------------------------------------------------------
find_package(GTest CONFIG QUIET)
if(GTest_FOUND)
    message(STATUS "Tests: using installed GoogleTest ${GTest_VERSION}")
else()
    message(STATUS "Tests: fetching GoogleTest")
    include(FetchContent)
    FetchContent_Declare(googletest
        URL https://github.com/google/googletest/archive/refs/tags/v1.15.2.tar.gz
        URL_HASH SHA256=7b42b4d6ed48810c5362c265a17faebe90dc2373c885e5216439d37927f02926
        DOWNLOAD_EXTRACT_TIMESTAMP TRUE
    )
    # Match the library's runtime (/MD) on MSVC and keep GoogleTest out of the install
    set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
    set(INSTALL_GTEST OFF CACHE BOOL "" FORCE)
    set(BUILD_GMOCK OFF CACHE BOOL "" FORCE)
    # The Conan toolchain sets BUILD_SHARED_LIBS=ON for the library.  GoogleTest must be static,
    # otherwise its DLLs are not beside the test executable and Windows can't start it.
    set(BUILD_SHARED_LIBS OFF)
    FetchContent_MakeAvailable(googletest)
endif()

# ---------------------------------------------------------------------------
# Static copy of the library for white-box testing
# ---------------------------------------------------------------------------
add_library(ims_testable STATIC ${ims_api_source_files})

get_target_property(_ims_includes ${IMS_TARGET_NAME} INCLUDE_DIRECTORIES)
get_target_property(_ims_defs     ${IMS_TARGET_NAME} COMPILE_DEFINITIONS)
get_target_property(_ims_libs     ${IMS_TARGET_NAME} LINK_LIBRARIES)
list(REMOVE_ITEM _ims_defs _EXPORTING_IMS _STATIC_IMS)

target_include_directories(ims_testable PUBLIC ${_ims_includes})
target_compile_definitions(ims_testable PUBLIC ${_ims_defs} _STATIC_IMS)
target_link_libraries(ims_testable PUBLIC ${_ims_libs})
set_target_properties(ims_testable PROPERTIES POSITION_INDEPENDENT_CODE ON)

if(NOT WIN32)
    find_package(Threads REQUIRED)
    target_link_libraries(ims_testable PUBLIC Threads::Threads ${CMAKE_DL_LIBS})
endif()

# ---------------------------------------------------------------------------
# Test executable
# ---------------------------------------------------------------------------
add_executable(ims_unit_tests
    support/CM_TEST.cpp
    test_reports.cpp
    test_cm_common.cpp
    test_rx_matching.cpp
    test_protocol_v2.cpp
)

# On Windows the hardware database and default settings are Win32 resources, looked up in the
# module that contains the code, so they must be linked into the test executable itself
if(WIN32)
    target_sources(ims_unit_tests PRIVATE ${api_db_resource})
endif()

target_include_directories(ims_unit_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/support)
target_link_libraries(ims_unit_tests PRIVATE ims_testable GTest::gtest_main)

# ---------------------------------------------------------------------------
# Code coverage (GCC/Clang): -DIMSLIB_COVERAGE=ON, then run the tests and gcovr.
# Optimisation is turned off for the instrumented code so lines and branches map to source.
# ---------------------------------------------------------------------------
option(IMSLIB_COVERAGE "Instrument the library and tests for code coverage (GCC/Clang)" OFF)
if(IMSLIB_COVERAGE)
    if(NOT CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
        message(FATAL_ERROR "IMSLIB_COVERAGE needs GCC or Clang")
    endif()
    foreach(_t ims_testable ims_unit_tests)
        target_compile_options(${_t} PRIVATE -O0 -g --coverage)
        target_link_options(${_t} PRIVATE --coverage)
    endforeach()
    message(STATUS "Tests: code coverage instrumentation on")
endif()

# Register each TEST() with ctest.  Discovery runs the executable after building it.
gtest_discover_tests(ims_unit_tests
    DISCOVERY_TIMEOUT 60
    PROPERTIES TIMEOUT 60
)
