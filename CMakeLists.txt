cmake_minimum_required(VERSION 3.14)
project(libims VERSION 1.8.5 DESCRIPTION "Isomet iMS Synthesiser Library")

# Min C++14
set(CMAKE_CXX_STANDARD 14)

set(LIBRARY_NAME "ims")

include(GNUInstallDirs)

##
## Check for LibXML2
##
find_package(LibXml2 2.9.4)
if (LibXml2_FOUND)
    message("-- Dynamically linking iMS Library to LibXml2")
    include_directories(${LIBXML2_INCLUDE_DIR})
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${LIBXML2_DEFINITIONS}")
else()
    include(FetchContent)
    FetchContent_Declare(ZLIB
         GIT_REPOSITORY https://github.com/madler/zlib
         GIT_TAG        v1.2.11
    )
    FetchContent_MakeAvailable(ZLIB)

    FetchContent_Declare(libxml2
         GIT_REPOSITORY https://github.com/robotology-dependencies/libxml2-cmake-buildsystem.git
         GIT_TAG        v2.9.9  
    )

    FetchContent_GetProperties(libxml2)

    if(NOT libxml2_POPULATED)
        message("-- Building static library LibXml2")
        message("-- Fetching LibXML2")
        FetchContent_Populate(libxml2)

        set(LIBXML2_WITH_ICONV OFF CACHE BOOL "" FORCE)
        set(LIBXML2_WITH_ZLIB ON CACHE BOOL "" FORCE)
        set(LIBXML2_WITH_LZMA OFF CACHE BOOL "" FORCE)
        set(LIBXML2_WITH_HTTP OFF CACHE BOOL "" FORCE)
        set(LIBXML2_WITH_FTP OFF CACHE BOOL "" FORCE)
        set(LIBXML2_WITH_HTML OFF CACHE BOOL "" FORCE)

        set(BUILD_SHARED_LIBS_SAVED "${BUILD_SHARED_LIBS}")
        set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)

        add_subdirectory(${libxml2_SOURCE_DIR} ${libxml2_BINARY_DIR})

        set(BUILD_SHARED_LIBS "${BUILD_SHARED_LIBS_SAVED}")

        find_package(LibXml2 2.9.9)
        if (LibXml2_FOUND)
            message("-- Statically linking iMS Library to LibXml2")
            include_directories(${LIBXML2_INCLUDE_DIR})
            set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${LIBXML2_DEFINITIONS}")
        else()
            message(FATAL_ERROR "-- Unable to build libxml2")
        endif()

    endif()

endif()

##
## Get BOOST
##
add_subdirectory(modules/boost-cmake)
set_property(
    TARGET 
        Boost_regex 
        Boost_log_setup
        Boost_log
        Boost_filesystem
        Boost_atomic
        Boost_date_time
        Boost_thread
        Boost_chrono    
    PROPERTY POSITION_INDEPENDENT_CODE ON)

# Missing compile definition in boost-cmake.  If not included, compilation of init_from_settings.cpp fails with 
# "Error: boost::condition_variable has no member 'timed_wait'
target_compile_definitions(Boost_log_setup PRIVATE
    BOOST_THREAD_USES_DATETIME
)

set(api_source_dir  	${CMAKE_CURRENT_SOURCE_DIR}/src)
set(api_include_dir 	${CMAKE_CURRENT_SOURCE_DIR}/inc)
set(api_resource_dir	${CMAKE_CURRENT_SOURCE_DIR}/res)
set(api_lib_dir	        ${CMAKE_CURRENT_SOURCE_DIR}/lib)

include_directories(AFTER 
    ${api_include_dir}
    ${api_resource_dir}
    ${api_lib_dir}/spline
    ${api_lib_dir}/sqlite3
    ${BOOST_SOURCE}
)

if(WIN32)
include_directories(AFTER 
    ${api_lib_dir}/enumser
    ${api_lib_dir}/FTDI
    ${api_lib_dir}/CyAPI/inc
)
endif()

# API Source Files
include(${CMAKE_CURRENT_SOURCE_DIR}/ims_api.cmake)

# Generate resources
if (NOT WIN32)
execute_process(
    COMMAND ./build.sh
    WORKING_DIRECTORY ${api_resource_dir}
    RESULT_VARIABLE XXD_ERR
)
if (XXD_ERR)
    MESSAGE( STATUS "Resource generation error:" ${XXD_ERR})
endif()
    set (api_db_resource ${api_resource_dir}/imshw.h)
else()
    set (api_db_resource ${api_resource_dir}/imshw.rc)
endif()

#
# Compiler specific Definitions
#

if(MSVC)
endif()

if(CMAKE_COMPILER_IS_GNUCXX)
endif()

if ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "Clang")
    if(APPLE)
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -stdlib=libc++")
    endif()
endif()

if("${CMAKE_BUILD_TYPE}" STREQUAL "Debug")
#    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O0 -Wall -Wextra -Wcast-qual -Wcast-align -Wno-unused-parameter -Wmissing-include-dirs -Wpointer-arith -Wredundant-decls -Wshadow -fprofile-arcs")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O0 -g3")
endif()

#
# Building and Linking
#
add_library(${LIBRARY_NAME} "")

target_sources(${LIBRARY_NAME} 
    PRIVATE 
        ${ims_api_source_files}
        ${api_db_resource}
        ${ims_api_header_files}
 )

target_include_directories(${LIBRARY_NAME}
    PUBLIC
        # where top-level project will look for the library's public headers
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        # where external projects will look for the library's public headers
        $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
)

target_link_libraries(${LIBRARY_NAME} 
    PRIVATE 
        -Wl,--whole-archive
            Boost::regex
            Boost::log_setup
            Boost::log
        -Wl,--no-whole-archive        
        ${LIBXML2_LIBRARIES}
)

set_target_properties(${LIBRARY_NAME} PROPERTIES VERSION ${PROJECT_VERSION})
set_target_properties(${LIBRARY_NAME} PROPERTIES PUBLIC_HEADER "${ims_public_api_header_files}")
set_target_properties(${LIBRARY_NAME} PROPERTIES DEBUG_POSTFIX "d")

#
# Set Install options
# https://decovar.dev/blog/2021/03/08/cmake-cpp-library/
if(DEFINED CMAKE_INSTALL_PREFIX_INITIALIZED_TO_DEFAULT)
    message(
        STATUS
        "CMAKE_INSTALL_PREFIX is not set\n"
        "Default value: ${CMAKE_INSTALL_PREFIX}\n"
        "Will set it to ${CMAKE_SOURCE_DIR}/install"
    )
    set(CMAKE_INSTALL_PREFIX
        "${CMAKE_SOURCE_DIR}/install"
        CACHE PATH "Where the library will be installed to" FORCE
    )
else()
    message(
        STATUS
        "CMAKE_INSTALL_PREFIX was already set\n"
        "Current value: ${CMAKE_INSTALL_PREFIX}"
    )
endif()

install(
    TARGETS ${LIBRARY_NAME}
    EXPORT "${LIBRARY_NAME}Targets"
#    COMPONENT ${LIBRARY_NAME} # must be here, not any line lower
    # these get default values from GNUInstallDirs, no need to set them
    #RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR} # bin
    #LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR} # lib
    #ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR} # lib
    PUBLIC_HEADER DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/${LIBRARY_NAME} # include
    INCLUDES DESTINATION ${CMAKE_INSTALL_INCLUDEDIR} # include
)

if (NOT ${BUILD_SHARED_LIBS})
# If building a static library, we must export Boost::regex and the user must link to it
# Otherwise we get a configuration error where Boost::regex is not part of the export set
install(
    TARGETS Boost_regex Boost_log_setup Boost_log Boost_atomic Boost_date_time Boost_filesystem Boost_thread Boost_chrono
    EXPORT "${LIBRARY_NAME}Targets"
)
endif()

install(
    FILES
    "${CMAKE_CURRENT_BINARY_DIR}/${LIBRARY_NAME}Config.cmake"
    "${CMAKE_CURRENT_BINARY_DIR}/${LIBRARY_NAME}ConfigVersion.cmake"
    DESTINATION cmake
 #   COMPONENT ${LIBRARY_NAME}
)

# generate and install export file
install(EXPORT "${LIBRARY_NAME}Targets"
    FILE "${LIBRARY_NAME}Targets.cmake"
    DESTINATION cmake
)
# generate the export targets for the build tree
export(EXPORT "${LIBRARY_NAME}Targets"
    FILE "${CMAKE_CURRENT_BINARY_DIR}/cmake/${LIBRARY_NAME}Targets.cmake"
)

include(CMakePackageConfigHelpers)

write_basic_package_version_file(
    "${CMAKE_CURRENT_BINARY_DIR}/${LIBRARY_NAME}ConfigVersion.cmake"
    VERSION "${PROJECT_VERSION}"
    COMPATIBILITY AnyNewerVersion
)
# create config file
configure_package_config_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/Config.cmake.in
    "${CMAKE_CURRENT_BINARY_DIR}/${LIBRARY_NAME}Config.cmake"
    INSTALL_DESTINATION cmake#/${LIBRARY_NAME}
)
install(FILES
    "${CMAKE_CURRENT_BINARY_DIR}/${LIBRARY_NAME}ConfigVersion.cmake"
    DESTINATION cmake
)

set(CPACK_PROJECT_NAME ${PROJECT_NAME})
set(CPACK_PROJECT_VERSION ${PROJECT_VERSION})
include(${CMAKE_CURRENT_SOURCE_DIR}/packaging.cmake)
