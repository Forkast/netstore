#include "server.hpp"

#include <algorithm>
#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <memory>
#include <string.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <iostream>
#include <unistd.h>


using namespace std;
using namespace std::filesystem;

int64_t maks(int64_t, int64_t);

Server::Server(const string & mcast_addr,
			   in_port_t cmd_port,
			   const string & directory,
			   unsigned max_space,
			   int timeout)
	: _mcast_addr{mcast_addr},
	  _cmd_port{cmd_port},
	  _directory{directory},
	  _max_space{max_space},
	  _timeout{timeout, 0}
{
	index_files();
	join_broadcast();
}

Server::~Server()
{
	close(_sock);
}

void
Server::run()
{
	fd_set rfds, wfds;

	FD_ZERO(&rfds);
	FD_ZERO(&wfds);
	FD_SET(_sock, &rfds);
	int max = _sock;
	timeval timeout = _timeout; //TODO: jakis sensowny timeout

	for (auto const & p : _data_socks) { //TODO: wydzielic do funkcji
		if (!p.conn) {
			if (p.socket > max)
				max = p.socket;
			FD_SET(p.socket, &rfds);
			cout << "set accepting TCP socket" << endl;
		} else if (p.cmd == READ) {
			if (p.sent) {
				if (p.file > max)
					max = p.file;
				FD_SET(p.file, &rfds);
				cout << "set reading file" << endl;
			} else {// czytam tylko jak poprzednia paczka została wysłana
				if (p.socket > max)
					max = p.socket;
				FD_SET(p.socket, &wfds);
				cout << "set writing tcp socket" << endl;
			}
		} else if (p.cmd == WRITE) {
			if (p.sent) {
				if (p.file > max)
					max = p.file;
				FD_SET(p.file, &wfds);
				cout << "set writing file" << endl;
			} else {
				if (p.socket > max)
					max = p.socket;
				FD_SET(p.socket, &rfds);
				cout << "set reading tcp socket" << endl;
			}
		}
// 		if (p.timeout.tv_sec < timeout.tv_sec) // TODO: dodac timeout
// 			timeout = p.timeout;
	}

	if (!_cmd_queue.empty()) {
		cout << "is not empty" << endl;
		FD_SET(_sock, &wfds);
	}

	int a = select(max + 1, &rfds, &wfds, nullptr, &timeout);
	cout << "select out\n";

	if (a == 0) {
		//for (//wywalic przeterminowane sockety TODO
	} else {
		if (FD_ISSET(_sock, &rfds)) {
			sockaddr_in remote_addr;
			socklen_t socklen = sizeof remote_addr;
			char b[MAX_UDP];
			int len = recvfrom(_sock, b, MAX_UDP, 0, (sockaddr *) &remote_addr, &socklen);
			remote_addr.sin_family = AF_INET;
			string buf = string(b, len);
			push_commands(buf, remote_addr);
			for (auto i : buf) {
				cout << hex << (uint8_t)i;
			}
			cout << endl;
// 			cmd->getCmd();
		}
		if (FD_ISSET(_sock, &wfds)) {
			cout << "writing on udp" << endl;
			string buf;
			auto cmd = _cmd_queue.front();
			_cmd_queue.pop();
			cmd->send(_sock);
		}
		for (auto & p : _data_socks) {
			if (FD_ISSET(p.file, &rfds)) {
				cout << "we can read from the file" << endl;
				read_file(p);
			}
			if (FD_ISSET(p.file, &wfds)) {
				cout << "we can read from the file" << endl;
				int a = write_file(p);
				if (a < 0) {
					p.todel = true;
				}
			}
			if (FD_ISSET(p.socket, &rfds)) {
				if (!p.conn) {
					cout << "time to accept connection" << endl;
					sockaddr_in addr;
					socklen_t len = sizeof addr;
					int s = accept(p.socket, (sockaddr *) &addr, &len);
					cout << "connection from: " << inet_ntoa(addr.sin_addr) << ":" << dec << ntohs(addr.sin_port) << endl;
					close(p.socket);
					p.socket = s;
					p.conn = true;
				} else {
					recv_file(p);
				}
			}
			if (FD_ISSET(p.socket, &wfds)) {
				cout << "socket is open so im sending!" << endl;
				int a = send_file(p);
				if (a < 0) {
					p.todel = true;
				}
			}
		}
		_data_socks.erase(
			std::remove_if(_data_socks.begin(), _data_socks.end(),
						[](const Socket & s) { return s.todel; }),
			_data_socks.end());
	}
}

void
Server::index_files()
{
	for (directory_entry const & f : directory_iterator(_directory)) {
		if (f.is_regular_file()) {
			_files.insert(f.path().filename());
			_space_used += f.file_size();
		}
	}
}

/* Serwer powinien podłączyć się do grupy rozgłaszania ukierunkowanego pod wskazanym adresem MCAST_ADDR. Serwer powinien nasłuchiwać na porcie CMD_PORT poleceń otrzymanych z sieci protokołem UDP także na swoim adresie unicast. Serwer powinien reagować na pakiety UDP zgodnie z protokołem opisanym wcześniej. */
void
Server::join_broadcast()
{
	sockaddr_in local_address;
	ip_mreq ip_mreq;
	_sock = socket(AF_INET, SOCK_DGRAM, 0);
	if (_sock < 0)
		syserr("socket");

	/* podpięcie się do grupy rozsyłania */
	ip_mreq.imr_interface.s_addr = htonl(INADDR_ANY);
	if (inet_aton(_mcast_addr.c_str(), &ip_mreq.imr_multiaddr) == 0)
		syserr("inet_aton");
	if (setsockopt(_sock, IPPROTO_IP, IP_ADD_MEMBERSHIP, (void*)&ip_mreq, sizeof ip_mreq) < 0)
		syserr("setsockopt");

	/* podpięcie się pod lokalny adres i port */
	local_address.sin_family = AF_INET;
	local_address.sin_addr.s_addr = htonl(INADDR_ANY);
	local_address.sin_port = htons(_cmd_port);
	if (bind(_sock, (struct sockaddr *)&local_address, sizeof local_address) < 0)
		syserr("bind");
}

void
Server::push_commands(const string & buf, sockaddr_in remote_addr)
{
	if (!strncmp(buf.c_str(), HELLO, strlen(HELLO))) {
		_cmd_queue.push(shared_ptr <Command> {new GoodDayCmd{buf,
															  remote_addr,
															  _mcast_addr,
															  space_left()}});
	} else if (!strncmp(buf.c_str(), LIST, strlen(LIST))) {
		auto files_it = _files.begin(); //TODO: obsluga wyszukiwania
		while (files_it != _files.end())
			_cmd_queue.push(shared_ptr <Command> {new MyListCmd{buf, remote_addr, files_it, _files.end()}});
	} else if (!strncmp(buf.c_str(), GET, strlen(GET))) {
		GetCmd get_cmd{buf, remote_addr};
		Socket sock;
		sock.file = open_file(get_cmd.file_name(), O_RDONLY); // brak pliku?
		if (sock.file < 0)
			syserr("opening file");
		int port = open_tcp_port(sock, READ);
		_data_socks.push_back(sock);
		_cmd_queue.push(shared_ptr <Command> (new ConnectMeCmd{buf,
													remote_addr,
													get_cmd.file_name(),
													port}));
	} else if (!strncmp(buf.c_str(), DEL, strlen(DEL))) {
		DelCmd cmd{buf, remote_addr};
		delete_file(cmd.file_name());
	} else if (!strncmp(buf.c_str(), ADD, strlen(ADD))) {
		AddCmd cmd{buf, remote_addr};
		if (cmd.requested_size() > space_left()) {
			_cmd_queue.push(shared_ptr <Command> (new NoWayCmd{buf,
														remote_addr,
														cmd.file_name()}));
		} else {
			Socket sock;
			sock.file = open_file(cmd.file_name(), O_WRONLY);
			int port = open_tcp_port(sock, WRITE);
			_cmd_queue.push(shared_ptr <Command> (new CanAddCmd{buf,
														remote_addr,
														port}));
		}
	}
}

int
Server::open_tcp_port(Socket & sock, int flag)
{
	sock.sent = true;
	sock.start_time = std::chrono::system_clock::now();
	sock.conn = false;
	sock.todel = false;
	sock.cmd = flag;
	sockaddr_in local_address;
	sock.socket = socket(AF_INET, SOCK_STREAM, 0);
	socklen_t len = sizeof local_address;

	local_address.sin_family = AF_INET;
	local_address.sin_addr.s_addr = htonl(INADDR_ANY);
	local_address.sin_port = 0;

	if (bind(sock.socket, (struct sockaddr *) &local_address, sizeof local_address) < 0)
		syserr("bind");

	if (getsockname(sock.socket, (struct sockaddr *)&local_address, &len) < 0)
		syserr("getsockname");

	cout << "nowy port to " << inet_ntoa(local_address.sin_addr) << ":" << ntohs(local_address.sin_port) << endl;

	if (listen(sock.socket, 2) < 0)
		syserr("listen");
	return ntohs(local_address.sin_port);
}

void
Server::read_and_parse()
{
	// commands: Hello -> GoodDay, List -> MyList, Get -> Connect_me, Del -> null, Add -> no_way / can_add
	
}

int
Server::send_file(Socket & sock)
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

void
Server::read_file(Socket & sock)
{
	int a = read(sock.file, sock.buf, MAX_UDP);
	if (a < 0)
		prnterr("reading file");
	sock.sent = false;
	sock.size = a;
}

int
Server::open_file(const char * name, int flags)
{
	auto got = _files.find(name);
	if (got != _files.end()) {
		cout << "opening: " << _directory / name << endl;
		return open((_directory / name).c_str(), flags);
	}
	return -1;
}

void
Server::delete_file(const char * name)
{
	auto got = _files.find(name);
	if (got != _files.end()) {
		cout << "removing: " << _directory / name << endl;
		directory_entry f{_directory / name};
		_space_used -= f.file_size();
		remove(_directory / name);
		_files.erase(name);
	}
}

void
Server::recv_file(Socket & sock)
{
	int a = recv(sock.socket, sock.buf, MAX_UDP, 0);
	if (a < 0)
		prnterr("receiving file");
	sock.sent = true;
	sock.size = a;
}

int
Server::write_file(Socket & sock)
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

uint64_t
Server::space_left()
{
	if (_space_used > _max_space)
		return 0;
	else
		return _max_space - _space_used;
}
