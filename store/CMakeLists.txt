cmake_minimum_required(VERSION 3.20)
project(store)

set(CMAKE_CXX_STANDARD 20)

include_directories(deps/spdlog-1.11.0/include)
add_subdirectory(deps/spdlog-1.11.0)

aux_source_directory(. SOURCES)
add_executable(${PROJECT_NAME} ${SOURCES})
