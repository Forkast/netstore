#pragma once
#include <endian.h>

class SimplCmd
{
protected:
	char[10] cmd;
	uint64_t cmd_seq;
	char[] data;

public:
	SimplCmd()
	{
		for (int i = 0; i < 10; i++) {
			cmd[i] = '\0';
		}
	}
	virtual void send() = 0;
};

// Rozpoznawanie listy serwerów w grupie - zapytanie
class HelloCmd : public SimplCmd
{
public:
	HelloCmd()
	: SimplCmd{}
	{
		cmd = "HELLO";
	}
};

class ListCmd : public SimplCmd
{
public:
	ListCmd()
	: SimplCmd{}
	{
		cmd = "LIST";
	}
};

class MyCmd : public SimplCmd
{
public:
	MyCmd()
	: SimplCmd{}
	{
		cmd = "MY_LIST";
	}
};

// Pobieranie pliku z serwera
class GetCmd : public SimplCmd
{
public:
	GetCmd()
	: SimplCmd{}
	{
		cmd = "GET";
	}
};

// Usuwanie pliku z serwera
class DelCmd : public SimplCmd
{
public:
	DelCmd()
	: SimplCmd{}
	{
		cmd = "DEL";
	}
};

class NWCmd : public SimplCmd
{
public:
	NWCmd()
	: SimplCmd{}
	{
		cmd = "NO_WAY";
	}
};


class CmplxCmd
{
protected:
	char[10] cmd;
	uint64_t cmd_seq;
	uint64_t param;
	char[] data;
};

// Rozpoznawanie listy serwerów w grupie - odpowiedz
class GoodCmd : public CmplxCmd
{
public:
	GoodCmd()
	: CmplxCmd{}
	{
		cmd = "GOOD_DAY";
	}
};

// 
class ConnCmd : public CmplxCmd
{
public:
	ConnCmd()
	: CmplxCmd{}
	{
		cmd = "CONNECT_ME";
	}
};

// Dodawanie pliku do grupy
class AddCmd : public CmplxCmd
{
public:
	AddCmd()
	: CmplxCmd{}
	{
		cmd = "ADD";
	}
};

class CanCmd : public CmplxCmd
{
public:
	CanCmd()
	: CmplxCmd{}
	{
		cmd = "CAN_ADD";
	}
};

htobe64();
be64toh();
