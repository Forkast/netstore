#include "client.hpp"

#include <algorithm>
#include <cctype>
#include <fcntl.h>
#include <memory>
#include <regex>
#include <sys/socket.h>
#include <unistd.h>

using namespace std;
using namespace std::filesystem;
using namespace std::chrono;

#define STDIN 0

Client::Client(const string & mcast_addr, in_port_t cmd_port, const string & directory, int timeout)
	: _mcast_addr{mcast_addr},
	  _cmd_port{cmd_port},
	  _directory{directory},
	  _timeout{timeout, 0}
{
	_multicast.sin_family = AF_INET;
	_multicast.sin_addr.s_addr = inet_addr(_mcast_addr.c_str());
	_multicast.sin_port = htons(_cmd_port);
	_exit = false;
	_current_dir = current_path();
	_stdin_lock = {0, 0};
	_locked = false;
	open_udp_sock();
}

int
Client::run()
{
	if (_exit) return 0;
	fd_set rfds, wfds;

	FD_ZERO(&rfds);
	FD_ZERO(&wfds);
	int max = 0;

	timeval * timeout = nullptr;
	timeval real_timeout{100000, 0};
	if (!_locked) {
		FD_SET(STDIN, &rfds);
	} else {
		int64_t passed = duration_cast <seconds> (system_clock::now() - _lock_time).count();
		if (passed > _timeout.tv_sec)
			real_timeout.tv_sec = 0;
		else
			real_timeout.tv_sec = _timeout.tv_sec - passed;
		timeout = &real_timeout;
	}

	for (auto const & p : _data_socks) {
		if (!p.conn) {
			if (p.sent) {
				if (p.cmd_socket > max)
					max = p.cmd_socket;
				FD_SET(p.cmd_socket, &rfds);
			} else {
				if (p.cmd_socket > max)
					max = p.cmd_socket;
				FD_SET(p.cmd_socket, &wfds);
			}
		} else {
			if (p.cmd == READ) {
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
		}

		if (!p.conn) {
			long int passed = duration_cast <seconds> (system_clock::now() - p.start_time).count();
			long int left = _timeout.tv_sec - passed;
			left = left < 0 ? 0 : left;
			if (real_timeout.tv_sec > left) {
				real_timeout.tv_sec = left;
				timeout = &real_timeout;
			}
		}
	}

	FD_SET(_udp_sock, &rfds);
	max = max < _udp_sock ? _udp_sock : max;

	if (!_cmd_queue.empty()) {
		FD_SET(_udp_sock, &wfds);
	}

	int a = select(max + 1, &rfds, &wfds, nullptr, timeout);

	if (a == 0) {
		int64_t passed = duration_cast <seconds> (system_clock::now() - _lock_time).count();
		if (passed > _timeout.tv_sec)
			_locked = false;

		for (auto & p : _data_socks) {
			if (!p.conn) {
				long int passed = duration_cast <seconds> (system_clock::now() - p.start_time).count();
				if (passed >= _timeout.tv_sec)
					todel(p);
			}
		}
	} else {
		if (FD_ISSET(STDIN, &rfds)) {
			char buf[MAX_UDP];
			int a = read(STDIN, buf, MAX_UDP);
			parse_command(string(buf, a));
		}
		if (FD_ISSET(_udp_sock, &wfds)) {
			auto cmd = _cmd_queue.front();
			_cmd_queue.pop();
			cmd->send(_udp_sock);
		}
		if (FD_ISSET(_udp_sock, &rfds)) {
			sockaddr_in remote;
			socklen_t len = sizeof remote;
			char buf[MAX_UDP];
			int a = recvfrom(_udp_sock, buf, MAX_UDP, 0, (sockaddr *) &remote, &len);
			parse_response(string(buf, a), remote);
		}
		for (auto & p : _data_socks) {
			if (!p.conn) {
				if (FD_ISSET(p.cmd_socket, &rfds)) {
					sockaddr_in remote;
					socklen_t len = sizeof remote;
					char buf[MAX_UDP];
					int a = recvfrom(p.cmd_socket, buf, MAX_UDP, 0, (sockaddr *) &remote, &len);
					parse_response_on_socket(string(buf, a), remote, p);
				}
				if (FD_ISSET(p.cmd_socket, &wfds)) {
					p.connect_cmd->send(p.cmd_socket);
					p.sent = true;
				}
			} else {
				if (FD_ISSET(p.file, &rfds)) {
					read_file(p);
				}
				if (FD_ISSET(p.file, &wfds)) {
					int a = write_file(p);
					if (a < 0) {
						cout << "File "
							<< p.filename
							<< "downloaded ("
							<< inet_ntoa(p.connect_cmd->getAddr().sin_addr)
							<< ":"
							<< ntohs(p.connect_cmd->getAddr().sin_port)
							<< ")" << endl;
						todel(p);
					}
				}
				if (FD_ISSET(p.socket, &rfds)) {
					recv_file(p);
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
	}
	return 1;
}

// cmd
// sscanf(buf.substr(buf.find() + cmd.size(), buf.size()), "%s", &filename);

void
Client::parse_command(const string & buf)
{
	smatch m;
	if (regex_match(buf, regex("^\\s*discover\\s*$"))) {
		_servers.clear();
		syncronous_command(1);
	} else if (regex_match(buf, m, regex("^\\s*search\\s*([^\\s]*)\\s*$"))) {
		char filename[MAX_BUF];
		sscanf(m[1].str().c_str(), "%s", filename);
		syncronous_command(2, filename);
	} else if (regex_match(buf, m, regex("^\\s*fetch\\s*([^\\s]+)\\s*$"))) {
		char filename[MAX_BUF];
		sscanf(m[1].str().c_str(), "%s", filename);
		if (_listed_filenames.find(filename) != _listed_filenames.end()) {
			Socket sock;
			sock.conn = false;
			sock.sent = false;
			sock.connect_cmd = shared_ptr <Command> {new GetCmd{_multicast,
																_cmd_seq,
																filename}};
			open_udp_sock(sock);
			_data_socks.push_back(sock);
		}
	} else if (regex_match(buf, m, regex("^\\s*upload\\s*([^\\s]+)\\s*$"))) {
		char filename[MAX_BUF];
		sscanf(m[1].str().c_str(), "%s", filename);
		uint64_t size = 0;
		path file;
		Socket sock;
		if (is_regular_file(filename)) {
			file = filename;
			if (!_servers.empty()) { //musi się zmieścić
				sockaddr_in best;
				sock.conn = false;
				sock.sent = false;

				for (const auto & serv : _servers) {
					if (serv.first > size) {
						best = serv.second;
						sock.connect_cmd = shared_ptr <Command> {new AddCmd{best, _cmd_seq, size, file.filename()}};
						sock.filename = file;
						break;
					}
				}
				
			}
		} else {
			cout << "File "
				<< filename
				<< " does not exist" << endl;
			return;
		}
		// TODO: a potem do następnego który ma miejsce
		if (sock.connect_cmd) {
			open_udp_sock(sock);
			_data_socks.push_back(sock);
		} else {
			cout << "File "
				<< filename
				<< " too big" << endl;
		}
	} else if (regex_match(buf, m, regex("^\\s*remove\\s*([^\\s]+)\\s*$"))) {
		char filename[MAX_BUF];
		sscanf(m[1].str().c_str(), "%s", filename);
		if (strlen(filename) != 0) {
			_cmd_queue.push(shared_ptr <Command> {new DelCmd{_multicast,
															_cmd_seq,
															filename}});
		}
	} else if (regex_match(buf, regex("^\\s*exit\\s*$"))) {
		_exit = true;
	}
}

void
Client::parse_response(const string & buf, sockaddr_in remote_addr)
{
	if (SimplCmd{buf, remote_addr}.getCmdSeq() != _cmd_seq)
		return;
	if (!strncmp(buf.c_str(), GOOD_DAY, strlen(GOOD_DAY))) {
		GoodDayCmd gdcmd{buf, remote_addr};

		_servers.insert(std::pair <uint64_t, sockaddr_in> {gdcmd.getSizeLeft(), remote_addr});
		cout << "Found "
			<< inet_ntoa(remote_addr.sin_addr)
			<< "("
			<< gdcmd.getMCastAddr()
			<< ") with free space "
			<< gdcmd.getSizeLeft() << endl;
	} else if (!strncmp(buf.c_str(), MY_LIST, strlen(MY_LIST))) {
		MyListCmd mlcmd{buf, remote_addr};
		std::string filename;
		std::istringstream tokenStream(mlcmd.getFileList());

		while (std::getline(tokenStream, filename))
		{
			_listed_filenames.insert(filename);
			cout << filename << " (" << inet_ntoa(remote_addr.sin_addr) << ")" << endl;
		}
	} else {
		cout << "[PCKG ERROR] Skipping invalid package from "
			<< inet_ntoa(remote_addr.sin_addr)
			<< ":"
			<< ntohs(remote_addr.sin_port)
			<< ".";
	}
}

void
Client::parse_response_on_socket(const string & buf, sockaddr_in remote_addr, Socket & sock)
{
	if (SimplCmd{buf, remote_addr}.getCmdSeq() != _cmd_seq)
		return;
	if (!strncmp(buf.c_str(), CONNECT_ME, strlen(CONNECT_ME))) {
		ConnectMeCmd cmd{buf, remote_addr};

		udp_socket_to_tcp(sock, remote_addr, cmd, WRITE);
		sock.file = open((_directory / cmd.file_name()).c_str(), O_WRONLY | O_CREAT, 0644);

	} else if (!strncmp(buf.c_str(), NO_WAY, strlen(NO_WAY))) {
		NoWayCmd cmd{buf, remote_addr}; //TODO: sprobowac z nastepnym
		cout << "File "
			<< cmd.filename()
			<< " downloading failed ("
			<< inet_ntoa(remote_addr.sin_addr)
			<< ":"
			<< ntohs(remote_addr.sin_port)
			<< ")" << endl;
	} else if (!strncmp(buf.c_str(), CAN_ADD, strlen(CAN_ADD))) {
		CanAddCmd cmd{buf, remote_addr};

		udp_socket_to_tcp(sock, remote_addr, cmd, READ);
		sock.file = open(sock.filename.c_str(), O_RDONLY);
		if (sock.file < 0)
			syserr("canadd open");

	} else {
		cout << "[PCKG ERROR] Skipping invalid package from "
			<< inet_ntoa(remote_addr.sin_addr)
			<< ":"
			<< ntohs(remote_addr.sin_port)
			<< ".";
	}
}

void
Client::syncronous_command(int parameter, const string & name)
{
	if (parameter == 1) {
		_cmd_queue.push(shared_ptr <Command> {new HelloCmd{_multicast, _cmd_seq}});
	} else {
		_cmd_queue.push(shared_ptr <Command> {new ListCmd{_multicast, _cmd_seq, name}});
		_listed_filenames.clear();
	}
	_lock_time = system_clock::now();
	_locked = true;
}

void
Client::open_udp_sock()
{
	sockaddr_in local_address;
	_udp_sock = socket(AF_INET, SOCK_DGRAM, 0);

	local_address.sin_family = AF_INET;
	local_address.sin_addr.s_addr = htonl(INADDR_ANY);
	local_address.sin_port = 0;

	if (bind(_udp_sock, (struct sockaddr *) &local_address, sizeof local_address) < 0)
		syserr("bind");
}

void
Client::open_udp_sock(Socket & sock)
{
	sockaddr_in local_address;
	sock.cmd_socket = socket(AF_INET, SOCK_DGRAM, 0);
	sock.todel = false;
	sock.start_time = system_clock::now();

	local_address.sin_family = AF_INET;
	local_address.sin_addr.s_addr = htonl(INADDR_ANY);
	local_address.sin_port = 0;

	if (bind(sock.cmd_socket, (struct sockaddr *) &local_address, sizeof local_address) < 0)
		syserr("bind");
}

void
Client::open_tcp_sock(Socket & sock, sockaddr_in remote_addr, int flag)
{
	sock.sent = true;
	sock.cmd = flag;
	sockaddr_in local_address;
	sock.socket = socket(AF_INET, SOCK_STREAM, 0);

	local_address.sin_family = AF_INET;
	local_address.sin_addr.s_addr = htonl(INADDR_ANY);
	local_address.sin_port = 0;

	if (bind(sock.socket, (sockaddr *) &local_address, sizeof local_address) < 0)
		syserr("bind");

	if (connect(sock.socket, (sockaddr *) &remote_addr, sizeof remote_addr) < 0)
		syserr("connect");
}

void
Client::udp_socket_to_tcp(Socket & sock, sockaddr_in remote_addr, const CmplxCmd & cmd, int read_write)
{
	sockaddr_in new_remote;
	new_remote.sin_family = AF_INET;
	new_remote.sin_addr = remote_addr.sin_addr;
	new_remote.sin_port = cmd.param();
	sock.conn = true;
	open_tcp_sock(sock, new_remote, read_write);
	close(sock.cmd_socket);
}
