cmake_minimum_required(VERSION 3.11)

project(client-server CXX)
set(CMAKE_CXX_STANDARD 17)

include(${CMAKE_BINARY_DIR}/conanbuildinfo.cmake)
conan_basic_setup(TARGETS)

add_compile_definitions(BOOST_BEAST_USE_STD_STRING_VIEW)

set(THREADS_PREFER_PTHREAD_FLAG ON)
find_package(Threads REQUIRED)

add_executable(server src/server.cpp)
add_executable(client src/client.cpp)

target_link_libraries(server CONAN_PKG::boost Threads::Threads)
target_link_libraries(client CONAN_PKG::boost Threads::Threads)

file(MAKE_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

add_custom_command(TARGET server POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
    ${CMAKE_SOURCE_DIR}/data
    ${CMAKE_BINARY_DIR}/bin
)