cmake_minimum_required(VERSION 3.8)
project(ResourceQueue)

set(CMAKE_CXX_STANDARD 14)

# Files related to the server
set(SOURCE_FILES_SERVER src/main.cpp
        include/Resource.h
        src/Reservation.cpp
        include/Reservation.h
        src/ResourceQueue.cpp
        include/ResourceQueue.h
        src/ReservationRequest.cpp
        include/ReservationRequest.h
        src/Connection.cpp
        include/Connection.h)

# Files related to the API
set(SOURCE_FILES_API src/QueueClient.cpp
        src/client.h)

# Create client API library
add_library(ResourceQueueAPI SHARED
        src/QueueClient.cpp
        include/QueueClient.h)

# Files related to client example
set(SOURCE_FILES_EXAMPLE_CLIENT examples/main.cpp include/QueueClient.h)

# Create executables
add_executable(ResourceQueueServer ${SOURCE_FILES_SERVER})
add_executable(ResourceQueueExampleClient ${SOURCE_FILES_EXAMPLE_CLIENT})

# Client library
target_link_libraries(ResourceQueueExampleClient ResourceQueueAPI)

include_directories(include)

set(THREADS_PREFER_PTHREAD_FLAG ON)
find_package(Threads REQUIRED)
target_link_libraries(ResourceQueueServer ${CMAKE_THREAD_LIBS_INIT})
target_link_libraries(ResourceQueueAPI ${CMAKE_THREAD_LIBS_INIT})
target_link_libraries(ResourceQueueExampleClient ${CMAKE_THREAD_LIBS_INIT})

find_package(Boost 1.64.0 COMPONENTS system coroutine serialization REQUIRED)
include_directories(${Boost_INCLUDE_DIRS})
target_link_libraries(ResourceQueueServer ${Boost_LIBRARIES})
target_link_libraries(ResourceQueueAPI ${Boost_LIBRARIES})
target_link_libraries(ResourceQueueExampleClient ${Boost_LIBRARIES})