#pragma once
#include "protocol.hpp"

#include <csignal>
#include <filesystem>
#include <fstream>
#include <memory>
#include <netinet/in.h>
#include <queue>
#include <string>
#include <time.h>
#include <unordered_set>
#include <vector>

void signal_handler(int signal);

class Server //TODO: komunikaty o błędach
{
public:
	Server(const std::string&, in_port_t, const std::string&, unsigned int = 52428800, int = 5);
	~Server();
	int run();

private:
	std::string _mcast_addr;
	in_port_t _cmd_port;
	std::filesystem::path _directory;
	unsigned _max_space;
	unsigned _space_used;
	timeval _timeout;
	std::unordered_set <std::string> _files;
	int _sock;

	// socket, timeout, file name, command
	std::vector <Socket> _data_socks;
	std::queue <std::shared_ptr <Command> > _cmd_queue;

private:
	void index_files();
	void join_broadcast();
	int open_file(const char * name, int flags);
	void delete_file(const char * name);
	void push_commands(const std::string & buf, sockaddr_in remote);
	int open_tcp_port(Socket & socket, int flag);
	uint64_t space_left();
};
