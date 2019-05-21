#pragma once

#include <chrono>
#include <filesystem>
#include <string>
#include <time.h>
#include <unordered_set>
#include <vector>

class Server
{
public:
	Server();
	~Server();
	void run();

private:
	string _mcast_addr;
	in_port_t _cmd_port;
	std::string _directory;
	unsigned _max_space;
	unsigned _space_used;
	timeval _timeout;
	std::unordered_set <const std::filesystem::directory_entry> _files;
	int _sock;

	struct Socket {
		int socket;
		timeval timeout;
		std::filesystem::directory_entry file;
		std::chrono::time_point start_time;
		std::fstream;
		int cmd;
	};

	// socket, timeout, file name, command
	std::vector <Socket> _data_socks;

private:
	void index_files(const std::string & directory);
	void join_broadcast();
	void read_and_parse();
}
