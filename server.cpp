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
using namespace std::chrono;

namespace {
	std::sig_atomic_t exit_program;
}

void signal_handler(int signal)
{
	exit_program = signal;
}

int64_t maks(int64_t, int64_t);

Server::Server(const string & mcast_addr,
			   in_port_t cmd_port,
			   const string & directory,
			   unsigned max_space,
			   int timeout,
			   bool synchronized)
	: _mcast_addr{mcast_addr},
	  _cmd_port{cmd_port},
	  _directory{directory},
	  _max_space{max_space},
	  _timeout{timeout, 0},
	  _synchronized{synchronized}
{
	index_files();
	join_broadcast();
}

Server::~Server()
{
	close(_sock);
}

int
Server::run()
{
	fd_set rfds, wfds;

	FD_ZERO(&rfds);
	FD_ZERO(&wfds);
	FD_SET(_sock, &rfds);
	int max = _sock;
	timeval * timeout = nullptr;
	system_clock::time_point first_time_point = system_clock::time_point::max();
	timeval real_timeout{0, 0};

	for (auto const & p : _data_socks) {
		if (!p.conn) {
			if (p.socket > max)
				max = p.socket;
			FD_SET(p.socket, &rfds);
		} else if (p.cmd == READ) {
			if (p.sent) {
				if (p.file > max)
					max = p.file;
				FD_SET(p.file, &rfds);
			} else {
				if (p.socket > max)
					max = p.socket;
				FD_SET(p.socket, &wfds);
			}
		} else if (p.cmd == WRITE) {
			if (p.sent) {
				if (p.socket > max)
					max = p.socket;
				FD_SET(p.socket, &rfds);
			} else {
				if (p.file > max)
					max = p.file;
				FD_SET(p.file, &wfds);
			}
		}
		if (p.start_time < first_time_point) {
			first_time_point = p.start_time;
		}
	}

	if (!_data_socks.empty()) {
		long int passed = duration_cast <seconds> (system_clock::now() - first_time_point).count();
		timeout = &real_timeout;
		real_timeout.tv_sec = _timeout.tv_sec - passed;
	}

	if (!_cmd_queue.empty()) {
		FD_SET(_sock, &wfds);
	}

	if (exit_program != 0) return 0;
	int a = select(max + 1, &rfds, &wfds, nullptr, timeout);
	if (exit_program != 0) return 0;

	if (a == 0) {
		for (auto & p : _data_socks) {
			long int passed = duration_cast <seconds> (system_clock::now() - p.start_time).count();
			if (passed >= _timeout.tv_sec)
				todel(p);
		}
	} else {
		if (FD_ISSET(_sock, &rfds)) {
			sockaddr_in remote_addr;
			socklen_t socklen = sizeof remote_addr;
			char b[MAX_UDP];
			int len = recvfrom(_sock, b, MAX_UDP, 0, (sockaddr *) &remote_addr, &socklen);
			remote_addr.sin_family = AF_INET;
			string buf = string(b, len);
			push_commands(buf, remote_addr);
		}
		if (FD_ISSET(_sock, &wfds)) {
			auto cmd = _cmd_queue.front();
			_cmd_queue.pop();
			cmd->send(_sock);
		}
		for (auto & p : _data_socks) {
			if (FD_ISSET(p.file, &rfds)) {
				read_file(p);
			}
			if (FD_ISSET(p.file, &wfds)) {
				int a = write_file(p);
				if (a < 0) {
					todel(p);
				}
			}
			if (FD_ISSET(p.socket, &rfds)) {
				if (!p.conn) {
					sockaddr_in addr;
					socklen_t len = sizeof addr;
					int s = accept(p.socket, (sockaddr *) &addr, &len);
					close(p.socket);
					p.socket = s;
					p.conn = true;
				} else {
					recv_file(p);
				}
			}
			if (FD_ISSET(p.socket, &wfds)) {
				int a = send_file(p);
				if (a < 0) {
					todel(p);
				}
			}
		}
	}
	_data_socks.erase(
		std::remove_if(_data_socks.begin(), _data_socks.end(),
					[](const Socket & s) { return s.todel; }),
		_data_socks.end());
	return 1;
}

void
Server::index_files()
{
	_space_used = 0;
	for (directory_entry const & f : directory_iterator(_directory)) {
		if (f.is_regular_file()) {
			_files.insert(f.path().filename());
			_space_used += f.file_size();
		}
	}
}

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
		ListCmd list_cmd{buf, remote_addr};
		uint64_t cmd_seq = list_cmd.getCmdSeq();
		auto files_it = _files.begin();
		string pattern{list_cmd.filename()};
		while (files_it != _files.end())
			_cmd_queue.push(shared_ptr <Command> {new MyListCmd{remote_addr, files_it, _files.end(), pattern, cmd_seq}});
	} else if (!strncmp(buf.c_str(), GET, strlen(GET))) {
		GetCmd get_cmd{buf, remote_addr};
		Socket sock;
		sock.file = open_file(get_cmd.file_name(), O_RDONLY);
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
		path file{_directory / cmd.file_name()};
		if (cmd.requested_size() > space_left()
			|| string(cmd.file_name()).find('/') != string::npos
			|| exists(file)) {
			_cmd_queue.push(shared_ptr <Command> (new NoWayCmd{buf,
														remote_addr,
														cmd.file_name()}));
		} else {
			Socket sock;
			sock.file = open_file(cmd.file_name(), O_WRONLY | O_CREAT);
			if (sock.file < 0)
				syserr("open");
			int port = open_tcp_port(sock, WRITE);
			_cmd_queue.push(shared_ptr <Command> (new CanAddCmd{buf,
														remote_addr,
														(in_port_t)port}));
			_data_socks.push_back(sock);
		}
	} else {
		cout << "[PCKG ERROR] Skipping invalid package from "
			<< inet_ntoa(remote_addr.sin_addr)
			<< ":"
			<< ntohs(remote_addr.sin_port)
			<< ".";
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

	if (listen(sock.socket, 2) < 0)
		syserr("listen");

	return ntohs(local_address.sin_port);
}

int
Server::open_file(const char * name, int flags)
{
	auto got = _files.find(name);
	if ((flags == (O_WRONLY | O_CREAT)) || got != _files.end()) {
		return open((_directory / name).c_str(), flags, 0644);
	}
	return -1;
}

void
Server::delete_file(const char * name)
{
	auto got = _files.find(name);
	if (got != _files.end()) {
		directory_entry f{_directory / name};
		_space_used -= f.file_size();
		remove(_directory / name);
		_files.erase(name);
	}
}

uint64_t
Server::space_left()
{
	if (_space_used > _max_space)
		return 0;
	else
		return _max_space - _space_used;
}
