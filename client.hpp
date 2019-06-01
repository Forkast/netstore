#pragma once
#include "protocol.hpp"

#include <csignal>
#include <filesystem>
#include <fstream>
#include <memory>
#include <netinet/in.h>
#include <queue>
#include <set>
#include <string>
#include <time.h>
#include <unordered_set>
#include <utility>
#include <vector>


class Client
{
public:
	Client(const std::string & mcast_addr, in_port_t cmd_port, const std::string & directory, int timeout);
	int run();

private:
	std::string _mcast_addr;
	in_port_t _cmd_port;
	std::filesystem::path _directory;
	std::filesystem::path _current_dir;
	timeval _timeout;
	timeval _stdin_lock;
	chrono::system_clock::time_point _lock_time;
	int _udp_sock;
	uint64_t _cmd_seq;
	sockaddr_in _multicast;
	bool _exit;
	bool _locked;
	std::string _file_waited_for;

	std::set < std::pair <uint64_t, sockaddr_in> > _servers;
	std::vector <Socket> _data_socks;
	std::queue <std::shared_ptr <Command> > _cmd_queue;
	std::set < std::pair <std::shared_ptr <Command>, std::filesystem::path> > _add_queue;
	std::unordered_set <std::shared_ptr <Command> > _get_queue;

	std::unordered_set <std::string> _listed_filenames;

private:
	void parse_command(const std::string & buf);
	void open_udp_sock();
	void syncronous_command(int param, const std::string & name = string{});
	void parse_response(const std::string & buf, sockaddr_in remote);
	void open_tcp_sock(Socket & sock, sockaddr_in remote_addr, int flags);
};
