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
#define MAX_BUF MAX_UDP - CMD_LEN - 16

#define syserr(x) {cerr << "Error making " << x << ". Code " << errno << " : " << strerror(errno) << endl; \
exit(0);}

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
		_data = new char[MAX_BUF];
		setAddr(remote);
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

	virtual void send(int sock) = 0;
};

class SimplCmd : public Command
{
public:
	SimplCmd(const string & s, sockaddr_in remote)
		: Command{s, remote}
	{
		int offset = CMD_LEN + sizeof _cmd_seq;
		memcpy(_data, s.c_str() + offset, s.size() - offset);
		_data[s.size() + offset] = '\0';
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
		memcpy(buf + offset, _data, MAX_BUF);
		if (sendto(sock, buf, MAX_UDP, 0, (const sockaddr *)&_addr, sizeof _addr) < 0)
			syserr("sendto");
		cout << "wyslane : " << buf << endl;
		cout << "na adres : " << inet_ntoa(_addr.sin_addr) << endl;
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
		memcpy(_cmd, MY_LIST, strlen(MY_LIST));
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
};

class GetCmd : public SimplCmd
{
public:
	GetCmd(const string & s, sockaddr_in remote)
		: SimplCmd{s, remote}
	{}

	const char * file_name()
	{
		return _data;
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
		sscanf(s.c_str() + CMD_LEN + sizeof _cmd_seq, "%lu", &_cmd_seq);
		int offset = CMD_LEN + sizeof _cmd_seq + sizeof _param;
		memcpy(_data, s.c_str() + offset, s.size() - offset);
		_data[s.size() + offset] = '\0';
	}

	virtual ~CmplxCmd() {}

	virtual void send(int sock)
	{
		cout << "ASDASDASDASDASDASDASDA" << endl;
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
		memcpy(buf + offset, _data, MAX_BUF);
		offset += strlen(_data);
		int a = 0;
		if ((a = sendto(sock, (char *)buf, offset, 0, (const sockaddr*)&_addr, sizeof _addr)) < 0)
			syserr("sendto");
		cout << "AFTER SENDTO : " << a << endl;
	}
};

class GoodDayCmd : public CmplxCmd
{
public:
	GoodDayCmd(const string & s, sockaddr_in remote, const string & mcast_addr, uint64_t size_left)
		: CmplxCmd{s, remote}
	{
		_param = size_left;
		memcpy(_data, mcast_addr.c_str(), mcast_addr.size());
	}
};

class ConnectMeCmd : public CmplxCmd
{
public:
	ConnectMeCmd(const std::string & s, sockaddr_in remote, const std::string & file_name)
		: CmplxCmd{s, remote}
	{
		memcpy(_data, file_name.c_str(), file_name.size());
		_data[file_name.size()] = '\0';
	}
};
