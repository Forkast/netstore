cmake_minimum_required(VERSION 3.9)
project(sikajnamnie)

#Bring the headers, such as Student.h into the project
#include_directories(include)

#Can manually add the sources using the set command as follows:
#set(SOURCES server.cpp protocol.cpp)

#However, the file(GLOB...) allows for wildcard additions:
file(GLOB SOURCES "*.cpp")


add_executable(server ${SOURCES})
set_property(TARGET server PROPERTY CXX_STANDARD 17)
target_link_libraries(server stdc++fs)
