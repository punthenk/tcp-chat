cmake_minimum_required(VERSION 4.1)
project(tcp_chat)

set(CMAKE_CXX_STANDARD 14)

add_executable(server src/server.cpp)
add_executable(client src/client.cpp)

