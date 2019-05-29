#pragma once
#include <string>
#include <filesystem>
#include <netinet/in.h>


class Client
{
public:
	Client(const std::string & mcast_addr, in_port_t cmd_port, const std::string & directory, int timeout);
	void run();

private:
	std::string _mcast_addr;
	in_port_t _cmd_port;
	std::filesystem::path _directory;
	timeval _timeout;
};
