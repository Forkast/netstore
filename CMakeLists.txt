cmake_minimum_required(VERSION 3.9)
project(sikajnamnie)

#Bring the headers, such as Student.h into the project
#include_directories(include)

#Can manually add the sources using the set command as follows:
set(SERVER_SOURCES server.cpp netstore-server.cpp protocol.cpp)
set(CLIENT_SOURCES client.cpp netstore-client.cpp protocol.cpp)

#However, the file(GLOB...) allows for wildcard additions:
#file(GLOB SOURCES "*server*.cpp")

add_executable(server ${SERVER_SOURCES})
set_property(TARGET server PROPERTY CXX_STANDARD 17)
target_link_libraries(server stdc++fs)

add_executable(client ${CLIENT_SOURCES})
set_property(TARGET client PROPERTY CXX_STANDARD 17)
target_link_libraries(client stdc++fs)
