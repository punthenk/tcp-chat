cmake_minimum_required(VERSION 4.1)
project(tcp_chat)

set(CMAKE_CXX_STANDARD 17)

add_executable(server src/server.cpp)
add_executable(client src/client.cpp)

add_executable(handshake-test
        src/DiffieHellman.cpp
        src/handshakeMain.cpp)
target_include_directories(handshake-test PRIVATE src)