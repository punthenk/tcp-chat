cmake_minimum_required(VERSION 4.1)
project(tcp_chat)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

find_package(OpenSSL REQUIRED)

add_executable(server src/server.cpp)
add_executable(client
        src/client.cpp
        src/DiffieHellman.cpp
        src/CryptoUtils.cpp)

add_executable(handshake-test
        src/DiffieHellman.cpp
        src/handshakeMain.cpp
        src/CryptoUtils.cpp)
target_include_directories(handshake-test PRIVATE src)

target_link_libraries(client OpenSSL::Crypto)
target_link_libraries(handshake-test OpenSSL::Crypto)
