cmake_minimum_required(VERSION 3.8)
project(ResourceQueue)

set(CMAKE_CXX_STANDARD 14)

set(SOURCE_FILES_SERVER server.cpp)
set(SOURCE_FILES_CLIENT client.cpp)
add_executable(ResourceQueueServer ${SOURCE_FILES_SERVER})
add_executable(ResourceQueueClient ${SOURCE_FILES_CLIENT})

set(THREADS_PREFER_PTHREAD_FLAG ON)
find_package(Threads REQUIRED)
target_link_libraries(ResourceQueueServer ${CMAKE_THREAD_LIBS_INIT})
target_link_libraries(ResourceQueueClient ${CMAKE_THREAD_LIBS_INIT})

find_package(Boost 1.64.0 COMPONENTS system coroutine REQUIRED)
include_directories(${Boost_INCLUDE_DIRS})
target_link_libraries(ResourceQueueServer ${Boost_LIBRARIES})
target_link_libraries(ResourceQueueClient ${Boost_LIBRARIES})