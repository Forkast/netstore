#include "client.hpp"

#include <algorithm>
#include <cctype>
#include <fcntl.h>
#include <memory>
#include <regex>
#include <sys/socket.h>
#include <unistd.h>
#include <random>


using namespace std;
using namespace std::filesystem;
using namespace std::chrono;

#define STDIN 0

uint64_t generate_cmd_seq()
{
	std::random_device rd;
	std::mt19937_64 gen(rd());

	std::uniform_int_distribution<uint64_t> dis;

	return dis(gen);
}

namespace {
	std::sig_atomic_t exit_program;
}

void signal_handler(int signal)
{
	exit_program = signal;
}

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
	_cmd_seq = generate_cmd_seq();
	_repeat_upload = false;
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

	if (exit_program != 0) return 0;
	int a = select(max + 1, &rfds, &wfds, nullptr, timeout);
	if (exit_program != 0) return 0;

	if (a == 0) {
		int64_t passed = duration_cast <seconds> (system_clock::now() - _lock_time).count();
		if (passed > _timeout.tv_sec) {
			_locked = false;
			if (_repeat_upload && !_servers.empty()) {
				Socket sock;
				path file{_repeat_filename};
				sock.filename = _repeat_filename;
				sock.size = file_size(file);
				sock.cmd = READ;
				sock.already_processed = 0;
				sock.start_time = system_clock::now();
				if (send_file_to_serv(sock)) {
					open_udp_sock(sock);
					_data_socks.push_back(sock);
				}
				_repeat_upload = false;
			}
		}

		for (auto & p : _data_socks) {
			if (!p.conn) {
				long int passed = duration_cast <seconds> (system_clock::now() - p.start_time).count();
				if (passed >= _timeout.tv_sec) {
					if (p.cmd == WRITE) {
						cout << "File "
							<< p.filename
							<< " downloading failed ("
							<< inet_ntoa(p.connect_cmd->getAddr().sin_addr)
							<< ":"
							<< ntohs(p.connect_cmd->getAddr().sin_port)
							<< ")" << endl;
					} else {
						cout << "File "
							<< (((AddCmd *)p.connect_cmd.get())->file_name())
							<< " too big" << endl;
					}
					todel(p, true);
				}
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
			memset(buf, 0, MAX_UDP);
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
						todel(p, true);
					}
				}
				if (FD_ISSET(p.socket, &rfds)) {
					recv_file(p);
				}
				if (FD_ISSET(p.socket, &wfds)) {
					int a = send_file(p);
					if (a < 0) {
						cout << "File "
							<< p.filename
							<< " uploaded ("
							<< inet_ntoa(p.connect_cmd->getAddr().sin_addr)
							<< ":"
							<< ntohs(p.connect_cmd->getAddr().sin_port)
							<< ")" << endl;
						todel(p, true);
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
	char filename[MAX_BUF];
	memset(filename, 0, MAX_BUF);
	if (regex_match(buf, regex("^\\s*discover\\s*$"))) {
		_servers.clear();
		syncronous_command(1);
	} else if (regex_match(buf, m, regex("^\\s*search\\s*([^\\s]*)\\s*$"))) {
		if (m.size() == 2) {
			sscanf(m[1].str().c_str(), "%s", filename);
		} else {
			memset(filename, 0, MAX_BUF);
		}
		syncronous_command(2, filename);
	} else if (regex_match(buf, m, regex("^\\s*fetch\\s*([^\\s]+)\\s*$"))) {
		sscanf(m[1].str().c_str(), "%s", filename);
		if (_listed_filenames.find(filename) != _listed_filenames.end()) {
			Socket sock;
			sock.cmd = WRITE;
			sock.already_processed = 0;
			sock.connect_cmd = shared_ptr <Command> {new GetCmd{_multicast,
																_cmd_seq,
																filename}};
			open_udp_sock(sock);
			_data_socks.push_back(sock);
		}
	} else if (regex_match(buf, m, regex("^\\s*upload\\s*([^\\s]+)\\s*$"))) {
		sscanf(m[1].str().c_str(), "%s", filename);
		path file;
		Socket sock;
		if (is_regular_file(filename)) {
			file = filename;
			sock.size = file_size(file);
			sock.filename = filename;
			sock.cmd = READ;
			sock.already_processed = 0;
			if (!_servers.empty()) {
				if (send_file_to_serv(sock)) {
					open_udp_sock(sock);
					_data_socks.push_back(sock);
				} else {
					cout << "File "
						<< filename
						<< " too big" << endl;
				}
			} else {
				syncronous_command(1);
				_repeat_upload = true;
				_repeat_filename = filename;
			}
		} else {
			cout << "File "
				<< filename
				<< " does not exist" << endl;
			return;
		}
	} else if (regex_match(buf, m, regex("^\\s*remove\\s*([^\\s]+)\\s*$"))) {
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
		print_invalid(remote_addr);
	}
}

void
Client::parse_response_on_socket(const string & buf, sockaddr_in remote_addr, Socket & sock)
{
	if (SimplCmd{buf, remote_addr}.getCmdSeq() != _cmd_seq) {
		print_invalid(remote_addr);
		return;
	}
	if (!strncmp(buf.c_str(), CONNECT_ME, strlen(CONNECT_ME))) {
		ConnectMeCmd cmd{buf, remote_addr};

		udp_socket_to_tcp(sock, remote_addr, cmd, WRITE);
		sock.file = open((_directory / cmd.file_name()).c_str(), O_WRONLY | O_CREAT, 0644);

	} else if (!strncmp(buf.c_str(), NO_WAY, strlen(NO_WAY))) {
		NoWayCmd cmd{buf, remote_addr};
		if (!send_file_to_serv(sock)) {
				cout << "File "
					<< cmd.filename()
					<< " uploading failed ("
					<< inet_ntoa(sock.connect_cmd->getAddr().sin_addr)
					<< ":"
					<< ntohs(sock.connect_cmd->getAddr().sin_port)
					<< ")" << endl;
			todel(sock, true);
		} else {
			open_udp_sock(sock);
		}
	} else if (!strncmp(buf.c_str(), CAN_ADD, strlen(CAN_ADD))) {
		CanAddCmd cmd{buf, remote_addr};

		udp_socket_to_tcp(sock, remote_addr, cmd, READ);
		sock.file = open(sock.filename.c_str(), O_RDONLY);
		if (sock.file < 0)
			syserr("canadd open");

	} else {
		print_invalid(remote_addr);
	}
}

//TODO fetch parse_response
/*
*/


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
	sock.conn = false;
	sock.sent = false;
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

void
Client::print_invalid(sockaddr_in remote_addr)
{
	cout << "[PCKG ERROR] Skipping invalid package from "
		<< inet_ntoa(remote_addr.sin_addr)
		<< ":"
		<< ntohs(remote_addr.sin_port)
		<< ".";
}

int
Client::send_file_to_serv(Socket & sock)
{
	sock.conn = false;
	sock.sent = false;

	int j = 0;
	for (const auto & serv : _servers) {
		if (serv.first > sock.size && j >= sock.already_processed) {
			path file{sock.filename};
			sock.connect_cmd = shared_ptr <Command> {new AddCmd{serv.second, _cmd_seq, sock.size, file.filename()}};
			sock.already_processed = j + 1;
			return 1;
		}
		j++;
	}
	return 0;
}
