#pragma once
#include <arpa/inet.h>
#include <endian.h>
#include <cstdint>
#include <cstring>
#include <ios>
#include <iostream>
#include <unordered_set>
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

#define syserr(x) {cerr << "Error making " << x << ". Code " << errno << " : " << strerror(errno) << endl; \
exit(0);}

#define prnterr(x) cerr << "Error making " << x << ". Code " << errno << " : " << strerror(errno) << endl;

class Command {
protected:
	char _cmd[CMD_LEN];
	uint64_t _cmd_seq;
	char * _data;
	sockaddr_in _addr;

	Command(const std::string & s, sockaddr_in remote)
	{
		memcpy(_cmd, s.c_str(), CMD_LEN);
		uint64_t temp = *(uint64_t *)(s.c_str() + CMD_LEN);
		setNetworkSeq(temp);
		_data = new char[BUF_SIZE];
		memset(_data, 0, BUF_SIZE);
		setAddr(remote);
	}

	Command(sockaddr_in remote, uint64_t cmd_seq)
	{
		_data = new char[BUF_SIZE];
		memset(_data, 0, BUF_SIZE);
		setAddr(remote);
		_cmd_seq = cmd_seq;
	}

public:
	virtual ~Command()
	{
		delete[] _data;
		_data = nullptr;
	}

	void getCmd() {
		for (int i = 0; i < CMD_LEN; i++) {
			cout << std::hex << (uint8_t)_cmd[i];
		}
		cout << endl;
		cout << hex << _cmd_seq << endl;
	}

	void setNetworkSeq(uint64_t seq) {
// htobe64();
		_cmd_seq = be64toh(seq);
	}

	void setAddr(sockaddr_in remote) {
		_addr = remote;
	}

	uint64_t getCmdSeq()
	{
		return _cmd_seq;
	}

	virtual void send(int sock) = 0;
};

class SimplCmd : public Command
{
public:
	SimplCmd(const string & s, sockaddr_in remote)
		: Command{s, remote}
	{
		int offset = CMD_LEN + sizeof _cmd_seq;
		strncpy(_data, s.c_str() + offset, MAX_BUF);
		_data[s.size()] = '\0';
	}

	SimplCmd(sockaddr_in remote, uint64_t cmd_seq)
		: Command{remote, cmd_seq}
	{
		
	}

	virtual ~SimplCmd() {}

	virtual void send(int sock)
	{
		uint8_t buf[MAX_UDP];
		int offset = 0;
		memcpy(buf, _cmd, CMD_LEN);
		offset += CMD_LEN;
		uint64_t temp = htobe64(_cmd_seq);
		memcpy(buf + offset, &temp, sizeof temp);
		offset += sizeof temp;
		memcpy(buf + offset, _data, MAX_UDP - offset);
		if (sendto(sock, buf, MAX_UDP, 0, (const sockaddr *)&_addr, sizeof _addr) < 0)
			syserr("sendto");
		cout << "na adres : " << inet_ntoa(_addr.sin_addr) << ":" << ntohs(_addr.sin_port) << endl;
	}
};

class HelloCmd : public SimplCmd
{
public:
	HelloCmd(const string & s, sockaddr_in remote)
		: SimplCmd{s, remote}
	{
		strncpy(_cmd, HELLO, CMD_LEN);
	}
	HelloCmd(sockaddr_in remote, uint64_t cmd_seq)
		: SimplCmd{remote, cmd_seq}
	{
		strncpy(_cmd, HELLO, CMD_LEN);
	}
};

class ListCmd : public SimplCmd
{
public:
	ListCmd(sockaddr_in remote, uint64_t cmd_seq)
		: SimplCmd{remote, cmd_seq}
	{
		strncpy(_cmd, LIST, CMD_LEN);
	}
};

class MyListCmd : public SimplCmd
{
public: //TODO pusty konstruktor
	MyListCmd(const std::string & s, sockaddr_in remote,
			  std::unordered_set <std::string>::iterator & file_names_it,
			  const std::unordered_set <std::string>::iterator & files_end)
		: SimplCmd{s, remote}
	{
		strncpy(_cmd, MY_LIST, CMD_LEN);
		size_t size = MAX_BUF;
		size_t offset = 0;
		while (file_names_it != files_end) {
			if ((*file_names_it).size() + 1 < size) {
				memcpy(_data + offset, (*file_names_it).c_str(), (*file_names_it).size());
				_data[offset + (*file_names_it).size()] = '\n';
				size -= (*file_names_it).size() + 1;
				offset += (*file_names_it).size() + 1;
			} else {
				break;
			}
			++file_names_it;
		}
	}

	MyListCmd(const std::string & s, sockaddr_in remote)
		: SimplCmd{s, remote}
	{}

	char * getFileList()
	{
		return _data;
	}
};

class GetCmd : public SimplCmd
{
public:
	GetCmd(const string & s, sockaddr_in remote)
		: SimplCmd{s, remote}
	{
		strncpy(_cmd, GET, CMD_LEN);
	}

	const char * file_name()
	{
		return _data;
	}
};

class DelCmd : public SimplCmd
{
public:
	DelCmd(const string & s, sockaddr_in remote)
		: SimplCmd{s, remote}
	{
		strncpy(_cmd, DEL, CMD_LEN);
	}

	const char * file_name()
	{
		return _data;
	}
};

class NoWayCmd : public SimplCmd
{
public:
	NoWayCmd(const string & s, sockaddr_in remote, const string & filename)
		: SimplCmd{s, remote}
	{
		strncpy(_cmd, NO_WAY, CMD_LEN);
		strncpy(_data, filename.c_str(), MAX_BUF);
	}
};

class CmplxCmd : public Command
{
protected:
	uint64_t _param;

public:
	CmplxCmd(const string & s, sockaddr_in remote)
		: Command{s, remote}
	{
		int offset = CMD_LEN + sizeof _cmd_seq;
		uint64_t temp = *(uint64_t *)(s.c_str() + offset);
		_param = be64toh(temp);
		offset += sizeof _param;
		memcpy(_data, s.c_str() + offset, s.size() - offset);
		_data[s.size()] = '\0';
	}

	virtual ~CmplxCmd() {}

	virtual void send(int sock)
	{
		uint8_t buf[MAX_UDP];
		int offset = 0;
		memcpy(buf, _cmd, CMD_LEN);
		offset += CMD_LEN;
		uint64_t temp = htobe64(_cmd_seq);
		memcpy(buf + offset, &temp, sizeof temp);
		offset += sizeof temp;
		temp = htobe64(_param);
		memcpy(buf + offset, &temp, sizeof temp);
		offset += sizeof temp;
		memcpy(buf + offset, _data, MAX_UDP - offset);
		offset += strlen(_data);
		int a = 0;
		if ((a = sendto(sock, (char *)buf, offset, 0, (const sockaddr*)&_addr, sizeof _addr)) < 0)
			syserr("sendto");
		for (int i = 0; i < offset; i++) {
			cout << hex << buf[i] ;
		}
		cout << endl;
	}
};

class GoodDayCmd : public CmplxCmd
{
public:
	GoodDayCmd(const std::string & s, sockaddr_in remote, const string & mcast_addr, uint64_t size_left)
		: CmplxCmd{s, remote}
	{
		strncpy(_cmd, GOOD_DAY, CMD_LEN);
		_param = size_left;
		memcpy(_data, mcast_addr.c_str(), mcast_addr.size());
	}

	GoodDayCmd(const std::string & s, sockaddr_in remote)
		: CmplxCmd{s, remote}
	{}

	uint64_t getSizeLeft()
	{
		return _param;
	}

	const char * getMCastAddr()
	{
		return _data;
	}
};

class ConnectMeCmd : public CmplxCmd
{
public:
	ConnectMeCmd(const std::string & s, sockaddr_in remote, const std::string & file_name, int port)
		: CmplxCmd{s, remote}
	{ //TODO: tak naprawdę tu będziemy chcieli wysłać zapytanie do reszty wezłów o istnienie takiego pliku.
		// jesli istnieje to tamten wezel sie zglosi do klienta :D
		strncpy(_cmd, CONNECT_ME, CMD_LEN);
		_param = ntohs(port);
		memcpy(_data, file_name.c_str(), file_name.size());
		_data[file_name.size()] = '\0';
	}
};

class AddCmd : public CmplxCmd
{
public:
	AddCmd(const std::string & s, sockaddr_in remote)
		: CmplxCmd{s, remote}
	{
		
	}

	uint64_t requested_size()
	{
		return _param;
	}

	const char * file_name()
	{
		return _data;
	}
};

class CanAddCmd : public CmplxCmd
{
public:
	CanAddCmd(const std::string & s, sockaddr_in remote, uint64_t port)
		: CmplxCmd{s, remote}
	{
		strncpy(_cmd, CAN_ADD, CMD_LEN);
		_param = ntohs(port);
	}
};
