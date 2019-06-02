CC = g++
CXXFLAGS = -lstdc++fs -std=c++17 -Wall -O2 -Wextra

all: server client

protocol:
	${CC} protocol.cpp -o protocol.o -c ${CXXFLAGS}

server: protocol
	${CC} netstore-server.cpp server.cpp protocol.o -o netstore-server ${CXXFLAGS}

client: protocol
	${CC} netstore-client.cpp client.cpp protocol.o -o netstore-client ${CXXFLAGS}

clean:
	rm -f *.o netstore-client netstore-server
