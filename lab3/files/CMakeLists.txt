cmake_minimum_required(VERSION 3.0.0)
project(RapidHTTP VERSION 0.1.0)

include(CTest)
enable_testing()

add_executable(RapidHTTP main.c server.c libsock.c)

set(CMAKE_C_FLAGS "-O3 ${CMAKE_CXX_FLAGS}")
set(CPACK_PROJECT_NAME ${PROJECT_NAME})
set(CPACK_PROJECT_VERSION ${PROJECT_VERSION})
include(CPack)
