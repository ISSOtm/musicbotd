cmake_minimum_required(VERSION 3.0)
project(musicbotd)

set(CMAKE_BUILD_TYPE Debug)

# MPV does not provide a CMake package
# find_package(mpv REQUIRED)
find_package(spdlog REQUIRED)
find_package(nlohmann_json REQUIRED)

set(FLAGS_ANY     "-Wall -Wextra -D_GNU_SOURCE -DSPDLOG_NO_THREAD_ID -DSPDLOG_NO_NAME -DMPV_ENABLE_DEPRECATED=0")
set(FLAGS_DEBUG   "-g -O0 -DSPDLOG_ACTIVE_LEVEL=SPDLOG_LEVEL_TRACE -fsanitize=undefined")
set(FLAGS_RELEASE "-O2 -DSPDLOG_ACTIVE_LEVEL=SPDLOG_LEVEL_INFO")

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_CXX_STANDARD 17)

set(CMAKE_CXX_FLAGS         "${CMAKE_CXX_FLAGS} ${FLAGS_ANY}")
set(CMAKE_CXX_FLAGS_DEBUG   "${CMAKE_CXX_FLAGS_DEBUG} ${FLAGS_ANY} ${FLAGS_DEBUG}")
set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} ${FLAGS_ANY} ${FLAGS_RELEASE}")

# Musicbot server executable
file(GLOB_RECURSE source_files src/*.cpp)
add_executable(${PROJECT_NAME} ${source_files})
target_link_libraries(${PROJECT_NAME}
    PRIVATE pthread mpv nlohmann_json::nlohmann_json
)
