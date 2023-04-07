if (NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE "Release" CACHE STRING "Choose Release or Debug" FORCE)
endif ()

cmake_minimum_required(VERSION 3.20)
project(pyskv)

set(CMAKE_CXX_STANDARD 20)
set(PYTHON_EXECUTABLE /usr/local/Caskroom/miniconda/base/bin/python)

include_directories(deps/spdlog-1.11.0/include)
add_subdirectory(deps/spdlog-1.11.0)
include_directories(deps/pybind11-2.10.3/include)
add_subdirectory(deps/pybind11-2.10.3)

aux_source_directory(. SOURCE)
pybind11_add_module(${PROJECT_NAME} SHARED ${SOURCE})

set(THREADS_PREFER_PTHREAD_FLAG ON)
find_package(Threads REQUIRED)
target_link_libraries(${PROJECT_NAME} PRIVATE Threads::Threads)