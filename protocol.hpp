#pragma once
#include <arpa/inet.h>
#include <chrono>
#include <cstdint>
#include <cstring>
#include <endian.h>
#include <ios>
#include <iostream>
#include <unordered_set>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/types.h>

using namespace std;

#define HELLO "HELLO"
#define LIST "LIST"
#define MY_LIST "MY_LIST"
#define GET "GET"
#define DEL "DEL"
#define NO_WAY "NO_WAY"
#define GOOD_DAY "GOOD_DAY"
#define CONNECT_ME "CONNECT_ME"
#define ADD "ADD"
#define CAN_ADD "CAN_ADD"
#define MAX_UDP 512
#define CMD_LEN 10
#define BUF_SIZE MAX_UDP + 1
#define MAX_BUF MAX_UDP - CMD_LEN - 16
#define READ 0
#define WRITE 1

#define syserr(x) {cerr << "Error making " << x << ". Code " << errno << " : " << strerror(errno) << endl; \
exit(0);}

#define prnterr(x) cerr << "Error making " << x << ". Code " << errno << " : " << strerror(errno) << endl;

struct Socket {
	int socket;
	std::chrono::system_clock::time_point start_time;
	int file;
	int cmd; // 0 - read, 1 - write
	char buf[MAX_UDP];
	int size;
	bool sent;
	bool conn;
	bool todel;
};

int write_file(Socket & sock);

void recv_file(Socket & sock);

int send_file(Socket & sock);

void read_file(Socket & sock);

void todel(Socket & sock);

class Command {
protected:
	char _cmd[CMD_LEN];
	uint64_t _cmd_seq;
	char * _data;
	sockaddr_in _addr;

	Command(const std::string & s, sockaddr_in remote);

	Command(sockaddr_in remote, uint64_t cmd_seq);

public:
	virtual ~Command();

	void getCmd();

	void setNetworkSeq(uint64_t seq);

	void setAddr(sockaddr_in remote);

	uint64_t getCmdSeq();

	virtual void send(int sock) = 0;
};

class SimplCmd : public Command
{
public:
	SimplCmd(const string & s, sockaddr_in remote);

	SimplCmd(sockaddr_in remote, uint64_t cmd_seq);

	virtual ~SimplCmd();

	virtual void send(int sock);
};

class HelloCmd : public SimplCmd
{
public:
	HelloCmd(const string & s, sockaddr_in remote);

	HelloCmd(sockaddr_in remote, uint64_t cmd_seq);
};

class ListCmd : public SimplCmd
{
public:
	ListCmd(sockaddr_in remote, uint64_t cmd_seq);
};

class MyListCmd : public SimplCmd
{
public:
	MyListCmd(const std::string & s, sockaddr_in remote,
			  std::unordered_set <std::string>::iterator & file_names_it,
			  const std::unordered_set <std::string>::iterator & files_end);

	MyListCmd(const std::string & s, sockaddr_in remote);

	char * getFileList();
};

class GetCmd : public SimplCmd
{
public:
	GetCmd(const string & s, sockaddr_in remote);

	GetCmd(sockaddr_in remote, uint64_t cmd_seq, const std::string & filename);

	const char * file_name();
};

class DelCmd : public SimplCmd
{
public:
	DelCmd(const string & s, sockaddr_in remote);

	DelCmd(sockaddr_in remote, uint64_t cmd_seq, const std::string & filename);

	const char * file_name();
};

class NoWayCmd : public SimplCmd
{
public:
	NoWayCmd(const string & s, sockaddr_in remote, const string & filename);
};

class CmplxCmd : public Command
{
protected:
	uint64_t _param;

public:
	CmplxCmd(const string & s, sockaddr_in remote);

	CmplxCmd(sockaddr_in remote, uint64_t cmd_seq);

	virtual ~CmplxCmd();

	virtual void send(int sock);
};

class GoodDayCmd : public CmplxCmd
{
public:
	GoodDayCmd(const std::string & s, sockaddr_in remote, const string & mcast_addr, uint64_t size_left);

	GoodDayCmd(const std::string & s, sockaddr_in remote);

	uint64_t getSizeLeft();

	const char * getMCastAddr();
};

class ConnectMeCmd : public CmplxCmd
{
public:
	ConnectMeCmd(const std::string & s, sockaddr_in remote, const std::string & file_name, int port);

	const char * file_name();

	uint64_t port();
};

class AddCmd : public CmplxCmd
{
public:
	AddCmd(const std::string & s, sockaddr_in remote);

	AddCmd(sockaddr_in remote, uint64_t cmd_seq, uint64_t size, const std::string & filename);

	uint64_t requested_size();

	const char * file_name();
};

class CanAddCmd : public CmplxCmd
{
public:
	CanAddCmd(const std::string & s, sockaddr_in remote, uint64_t port);
};
