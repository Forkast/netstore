#pragma once
#include <string>
#include <filesystem>
#include <netinet/in.h>


class Client
{
public:
	Client(const std::string & mcast_addr, in_port_t cmd_port, const std::string & directory, int timeout);
	int run();

private:
	std::string _mcast_addr;
	in_port_t _cmd_port;
	std::filesystem::path _directory;
	timeval _timeout;
	int _udp_sock;
	uint64_t _cmd_seq;
	sockaddr_in _multicast;
	bool _exit;

	std::vector <Socket> _data_socks;
	std::queue <std::shared_ptr <Command> > _cmd_queue;

private:
	void parse_command(const std::string & buf);
	void open_udp_sock();
	void syncronous_command(int param, const std::string & name);
};
