#pragma once
#include "protocol.hpp"

#include <chrono>
#include <filesystem>
#include <memory>
#include <netinet/in.h>
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
	//std::unordered_set <std::filesystem::directory_entry> _files;
	int _sock;

	struct Socket {
		int socket;
		timeval timeout;
		std::filesystem::directory_entry file;
// 		std::chrono::time_point start_time;
// 		std::ifstream stream;
		int cmd;
	};

	// socket, timeout, file name, command
	std::vector <Socket> _data_socks;

private:
	void index_files();
	void join_broadcast();
	void read_and_parse();
	void send_file(Socket socket);
	Command * get_command(const std::string & buf);
	void getCmdSeq(shared_ptr<Command> & cmd);
};
