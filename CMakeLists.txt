cmake_minimum_required(VERSION 3.0)
project(musicbotd)

# MPV does not provide a CMake package
# find_package(mpv REQUIRED)
find_package(spdlog REQUIRED)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_CXX_FLAGS "-std=c++17 -D_GNU_SOURCE -Wall -Wextra ${CMAKE_CXX_FLAGS}")

# Musicbot server executable
file(GLOB_RECURSE source_files src/*.cpp)
add_executable(${PROJECT_NAME} ${source_files})
target_link_libraries(${PROJECT_NAME}
    pthread
    mpv
)
