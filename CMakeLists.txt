cmake_minimum_required(VERSION 3.0.0)
project(ims-lib VERSION 1.8.5)

include(CTest)
enable_testing()

add_library(ims-lib ims-lib.cpp)

set(CPACK_PROJECT_NAME ${PROJECT_NAME})
set(CPACK_PROJECT_VERSION ${PROJECT_VERSION})
include(CPack)
