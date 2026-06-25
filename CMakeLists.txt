cmake_minimum_required(VERSION 3.16)
project(redis_clone CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type")

add_executable(redis_clone
    src/server.cpp
    src/store.cpp
    src/pubsub.cpp
    src/parser.cpp
    src/commands.cpp
    src/client.cpp
    src/persistence.cpp
)

target_compile_options(redis_clone PRIVATE -Wall -Wextra -O2)
target_link_libraries(redis_clone PRIVATE pthread)
