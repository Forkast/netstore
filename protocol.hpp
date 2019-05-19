#pragma once
#include <endian.h>

class SimplCmd
{
protected:
	char[10] cmd;
	uint64_t cmd_seq;
	char[] data;

public:
	SimplCmd();
	virtual void send() = 0;
};

// Rozpoznawanie listy serwerów w grupie - zapytanie
class HelloCmd : public SimplCmd
{
public:
	HelloCmd();
};

class ListCmd : public SimplCmd
{
public:
	ListCmd();
};

class MyCmd : public SimplCmd
{
public:
	MyCmd();
};

// Pobieranie pliku z serwera
class GetCmd : public SimplCmd
{
public:
	GetCmd();
};

// Usuwanie pliku z serwera
class DelCmd : public SimplCmd
{
public:
	SimplCmd();
};

class NWCmd : public SimplCmd
{
public:
	NWCmd();
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
	GoodCmd();
};

// 
class ConnCmd : public CmplxCmd
{
public:
	ConnCmd();
};

// Dodawanie pliku do grupy
class AddCmd : public CmplxCmd
{
public:
	AddCmd();
};

class CanCmd : public CmplxCmd
{
public:
	CanCmd();
};

htobe64();
be64toh();
