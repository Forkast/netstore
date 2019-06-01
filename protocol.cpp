#include "protocol.hpp"

int write_file(Socket & sock)
{
	if (sock.size == 0) {
		return -1;
	}
	int a = write(sock.file, sock.buf, sock.size);
	if (a < 0)
		prnterr("writing file");
	sock.sent = false;
	sock.size = 0;
	return 1;
}

void recv_file(Socket & sock)
{
	int a = recv(sock.socket, sock.buf, MAX_UDP, 0);
	if (a < 0)
		prnterr("receiving file");
	sock.sent = true;
	sock.size = a;
}

int send_file(Socket & sock)
{
	if (sock.size == 0) {
		return -1;
	}
	int a = send(sock.socket, sock.buf, sock.size, 0);
	if (a < 0)
		prnterr("sending file");
	sock.sent = true;
	sock.size = 0;
	return 1;
}

void read_file(Socket & sock)
{
	int a = read(sock.file, sock.buf, MAX_UDP);
	if (a < 0)
		prnterr("reading file");
	sock.sent = false;
	sock.size = a;
}

void todel(Socket & sock)
{
	sock.todel = true;
	close(sock.file);
	close(sock.socket);
}

Command::Command(const std::string & s, sockaddr_in remote)
{
	memcpy(_cmd, s.c_str(), CMD_LEN);
	uint64_t temp = *(uint64_t *)(s.c_str() + CMD_LEN);
	setNetworkSeq(temp);
	_data = new char[BUF_SIZE];
	memset(_data, 0, BUF_SIZE);
	setAddr(remote);
}

Command::Command(sockaddr_in remote, uint64_t cmd_seq)
{
	_data = new char[BUF_SIZE];
	memset(_data, 0, BUF_SIZE);
	setAddr(remote);
	_cmd_seq = cmd_seq;
}

 Command::~Command()
{
	delete[] _data;
	_data = nullptr;
}

void
Command::getCmd() {
	for (int i = 0; i < CMD_LEN; i++) {
		cout << std::hex << (uint8_t)_cmd[i];
	}
	cout << endl;
	cout << hex << _cmd_seq << endl;
}

void
Command::setNetworkSeq(uint64_t seq) {
// htobe64();
	_cmd_seq = be64toh(seq);
}

void
Command::setAddr(sockaddr_in remote) {
	_addr = remote;
}

uint64_t
Command::getCmdSeq()
{
	return _cmd_seq;
}

SimplCmd::SimplCmd(const string & s, sockaddr_in remote)
	: Command{s, remote}
{
	int offset = CMD_LEN + sizeof _cmd_seq;
	strncpy(_data, s.c_str() + offset, MAX_BUF);
	_data[s.size()] = '\0';
}

SimplCmd::SimplCmd(sockaddr_in remote, uint64_t cmd_seq)
	: Command{remote, cmd_seq}
{}


SimplCmd::~SimplCmd() {}

 void
SimplCmd::send(int sock)
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

uint64_t
SimplCmd::getCmdSeq()
{
	return _cmd_seq;
}

HelloCmd::HelloCmd(const string & s, sockaddr_in remote)
	: SimplCmd{s, remote}
{
	strncpy(_cmd, HELLO, CMD_LEN);
}

HelloCmd::HelloCmd(sockaddr_in remote, uint64_t cmd_seq)
	: SimplCmd{remote, cmd_seq}
{
	strncpy(_cmd, HELLO, CMD_LEN);
}


ListCmd::ListCmd(sockaddr_in remote, uint64_t cmd_seq)
	: SimplCmd{remote, cmd_seq}
{
	strncpy(_cmd, LIST, CMD_LEN);
}

MyListCmd::MyListCmd(const std::string & s, sockaddr_in remote,
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

MyListCmd::MyListCmd(const std::string & s, sockaddr_in remote)
	: SimplCmd{s, remote}
{}

char *
MyListCmd::getFileList()
{
	return _data;
}

GetCmd::GetCmd(const string & s, sockaddr_in remote)
	: SimplCmd{s, remote}
{
	strncpy(_cmd, GET, CMD_LEN);
}

GetCmd::GetCmd(sockaddr_in remote, uint64_t cmd_seq, const std::string & filename)
	: SimplCmd{remote, cmd_seq}
{
	strncpy(_cmd, GET, CMD_LEN);
	memcpy(_data, filename.c_str(), filename.size());
}

const char *
GetCmd::file_name()
{
	return _data;
}

DelCmd::DelCmd(const string & s, sockaddr_in remote)
	: SimplCmd{s, remote}
{
	strncpy(_cmd, DEL, CMD_LEN);
}

DelCmd::DelCmd(sockaddr_in remote, uint64_t cmd_seq, const std::string & filename)
	: SimplCmd{remote, cmd_seq}
{
	strncpy(_cmd, DEL, CMD_LEN);
	memcpy(_data, filename.c_str(), filename.size());
}

const char *
DelCmd::file_name()
{
	return _data;
}

NoWayCmd::NoWayCmd(const string & s, sockaddr_in remote)
	: SimplCmd{s, remote}
{
	strncpy(_cmd, NO_WAY, CMD_LEN);
}

NoWayCmd::NoWayCmd(const string & s, sockaddr_in remote, const string & filename)
	: SimplCmd{s, remote}
{
	strncpy(_cmd, NO_WAY, CMD_LEN);
	strncpy(_data, filename.c_str(), MAX_BUF);
}

CmplxCmd::CmplxCmd(const string & s, sockaddr_in remote)
	: Command{s, remote}
{
	int offset = CMD_LEN + sizeof _cmd_seq;
	uint64_t temp = *(uint64_t *)(s.c_str() + offset);
	_param = be64toh(temp);
	offset += sizeof _param;
	memcpy(_data, s.c_str() + offset, s.size() - offset);
	_data[s.size()] = '\0';
}

CmplxCmd::CmplxCmd(sockaddr_in remote, uint64_t cmd_seq)
	: Command{remote, cmd_seq}
{}


CmplxCmd::~CmplxCmd() {}

 void
CmplxCmd::send(int sock)
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

GoodDayCmd::GoodDayCmd(const std::string & s, sockaddr_in remote, const string & mcast_addr, uint64_t size_left)
	: CmplxCmd{s, remote}
{
	strncpy(_cmd, GOOD_DAY, CMD_LEN);
	_param = size_left;
	memcpy(_data, mcast_addr.c_str(), mcast_addr.size());
}

GoodDayCmd::GoodDayCmd(const std::string & s, sockaddr_in remote)
	: CmplxCmd{s, remote}
{}

uint64_t
GoodDayCmd::getSizeLeft()
{
	return _param;
}

const char *
GoodDayCmd::getMCastAddr()
{
	return _data;
}


ConnectMeCmd::ConnectMeCmd(const std::string & s, sockaddr_in remote)
	: CmplxCmd{s, remote}
{
	
}

ConnectMeCmd::ConnectMeCmd(const std::string & s, sockaddr_in remote, const std::string & file_name, int port)
	: CmplxCmd{s, remote}
{ //TODO: tak naprawdę tu będziemy chcieli wysłać zapytanie do reszty wezłów o istnienie takiego pliku.
	// jesli istnieje to tamten wezel sie zglosi do klienta :D
	strncpy(_cmd, CONNECT_ME, CMD_LEN);
	_param = ntohs(port);
	memcpy(_data, file_name.c_str(), file_name.size());
	_data[file_name.size()] = '\0';
}

const char *
ConnectMeCmd::file_name()
{
	return _data;
}

uint64_t
ConnectMeCmd::port()
{
	return _param;
}

AddCmd::AddCmd(const std::string & s, sockaddr_in remote)
	: CmplxCmd{s, remote}
{
	strncpy(_cmd, ADD, CMD_LEN);
}

AddCmd::AddCmd(sockaddr_in remote, uint64_t cmd_seq, uint64_t size, const std::string & filename)
	: CmplxCmd{remote, cmd_seq}
{
	strncpy(_cmd, ADD, CMD_LEN);
	_param = size;
	memcpy(_data, filename.c_str(), filename.size());
}

uint64_t
AddCmd::requested_size()
{
	return _param;
}

const char *
AddCmd::file_name()
{
	return _data;
}

CanAddCmd::CanAddCmd(const std::string & s, sockaddr_in remote)
	: CmplxCmd{s, remote}
{
	strncpy(_cmd, CAN_ADD, CMD_LEN);
}

CanAddCmd::CanAddCmd(const std::string & s, sockaddr_in remote, uint64_t port)
	: CmplxCmd{s, remote}
{
	strncpy(_cmd, CAN_ADD, CMD_LEN);
	_param = ntohs(port);
}

const char *
CanAddCmd::filename()
{
	return _data;
}
