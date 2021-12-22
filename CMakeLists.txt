cmake_minimum_required(VERSION 3.12)
project(libims VERSION 1.8.5 DESCRIPTION "Isomet iMS Synthesiser Library")

# Min C++14
set(CMAKE_CXX_STANDARD 14)

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
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O0 -Wall -Wextra -Wcast-qual -Wcast-align -Wno-unused-parameter -Wmissing-include-dirs -Wpointer-arith -Wredundant-decls -Wshadow -fprofile-arcs")
endif()

#
# Building and Linking
#
add_library(ims
    ${ims_api_header_files}
    ${ims_api_source_files}
	${api_resource_dir}/imshw.rc
)

target_link_libraries(ims 
    PUBLIC Boost::regex
    ${LIBXML2_LIBRARIES}
)

set_target_properties(ims PROPERTIES VERSION ${PROJECT_VERSION})

set(CPACK_PROJECT_NAME ${PROJECT_NAME})
set(CPACK_PROJECT_VERSION ${PROJECT_VERSION})
include(CPack)
