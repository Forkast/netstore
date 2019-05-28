#pragma once
#include "protocol.hpp"

#include <chrono>
#include <filesystem>
#include <fstream>
#include <memory>
#include <netinet/in.h>
#include <queue>
#include <string>
#include <time.h>
#include <unordered_set>
#include <vector>

class Server
{
public:
	Server(const std::string&, in_port_t, const std::string&, unsigned int = 52428800, int = 5);
	~Server();
	void run();

private:
	std::string _mcast_addr;
	in_port_t _cmd_port;
	std::string _directory;
	unsigned _max_space;
	unsigned _space_used;
	timeval _timeout;
	std::unordered_set <std::string> _files;
	int _sock;

	struct Socket {
		int socket;
		std::chrono::system_clock::time_point start_time;
		std::shared_ptr <std::ifstream> stream; // TODO:deskryptory plikow :/
		int cmd;
	};

	// socket, timeout, file name, command
	std::vector <Socket> _data_socks;
	std::queue <std::shared_ptr <Command> > _cmd_queue;

private:
	void index_files();
	void join_broadcast();
	void read_and_parse();
	void send_file(Socket socket);
	void push_commands(const std::string & buf, sockaddr_in remote);
	void open_tcp_port(Socket & socket);
};
