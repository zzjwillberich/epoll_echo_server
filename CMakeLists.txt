cmake_minimum_required(VERSION 3.10)
project(epoll_echo_server)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include_directories(include)

add_executable(do
    src/server.cpp
    src/threadspool.cpp
)

# 链接 pthread 库（线程支持）
target_link_libraries(do PRIVATE pthread)
