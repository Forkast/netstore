#pragma once
#include <endian.h>
#include <cstdint>
#include <cstring>
#include <ios>
#include <iostream>

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

class Command {
public:
	const static int MAX = 10;
protected:
	char cmd[Command::MAX];
	uint64_t cmd_seq;
	char data[];

public:
	virtual ~Command() {};
	void getCmd() {
		for (int i = 0; i < MAX; i++) {
			cout << std::hex << (uint8_t)cmd[i];
		}
		cout << endl;
		cout << hex << cmd_seq << endl;
	}
	void setNetworkSeq(uint64_t seq) {
// htobe64();
		cmd_seq = be64toh(seq);
	}
};

class SimplCmd : public Command
{
public:
	SimplCmd()
	{
		for (int i = 0; i < Command::MAX; i++) {
			cmd[i] = '\0';
		}
	}
	virtual ~SimplCmd() {};
	virtual void send() {}; //TODO pure
};

// Rozpoznawanie listy serwerów w grupie - zapytanie
class HelloCmd : public SimplCmd
{
public:
	HelloCmd()
	: SimplCmd{}
	{
		strcpy(cmd, HELLO);
	}
};

class ListCmd : public SimplCmd
{
public:
	ListCmd()
	: SimplCmd{}
	{
		strcpy(cmd, LIST);
	}
};

class MyCmd : public SimplCmd
{
public:
	MyCmd()
	: SimplCmd{}
	{
		strcpy(cmd, MY_LIST);
	}
};

// Pobieranie pliku z serwera
class GetCmd : public SimplCmd
{
public:
	GetCmd()
	: SimplCmd{}
	{
		strcpy(cmd, GET);
	}
};

// Usuwanie pliku z serwera
class DelCmd : public SimplCmd
{
public:
	DelCmd()
	: SimplCmd{}
	{
		strcpy(cmd, DEL);
	}
};

class NWCmd : public SimplCmd
{
public:
	NWCmd()
	: SimplCmd{}
	{
		strcpy(cmd, NO_WAY);
	}
};


class CmplxCmd : public Command
{
};

// Rozpoznawanie listy serwerów w grupie - odpowiedz
class GoodCmd : public CmplxCmd
{
public:
	GoodCmd()
	: CmplxCmd{}
	{
		strcpy(cmd, GOOD_DAY); 
	}
};

// 
class ConnCmd : public CmplxCmd
{
public:
	ConnCmd()
	: CmplxCmd{}
	{
		strcpy(cmd, CONNECT_ME);
	}
};

// Dodawanie pliku do grupy
class AddCmd : public CmplxCmd
{
public:
	AddCmd()
	: CmplxCmd{}
	{
		strcpy(cmd, ADD);
	}
};

class CanCmd : public CmplxCmd
{
public:
	CanCmd()
	: CmplxCmd{}
	{
		strcpy(cmd, CAN_ADD);
	}
};
