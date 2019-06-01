#include "client.hpp"

#include <algorithm>
#include <cctype>
#include <memory>
#include <unistd.h>
#include <sys/socket.h>

using namespace std;
using namespace std::filesystem;
using namespace std::chrono;

#define STDIN 0

//TODO: walidacja cmd_seq
//TODO: program raczej nie powinien wychodzic na bledzie
//TODO: czy plik ktory chcialem wyslac to ten ktory serwer chce odebrac?

bool operator/=(const string & s1, const string & s2)
{
	int i1 = 0;
	int i2 = 0;

	while (i1 < s1.size() && isspace(s1[i1])) i1++;
	while (i2 < s2.size() && isspace(s2[i2])) i2++;

	if (s2.size() == i2 && !s1.size() == i1) return false;

	while (i1 < s1.size() && i2 < s2.size()) {
		if (toupper(s1[i1]) != toupper(s2[i2])) return false;
		i1++;
		i2++;
	}

	if (s1.size() == i1) return true;
	return false;
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
	if (!_locked) {
		FD_SET(STDIN, &rfds);
	} else {// NOTE: sie nie kompiluje
		int64_t passed = time_point_cast <seconds> (_lock_time
													- system_clock::now()
													).count();
		if (passed > _timeout.tv_sec)
			_stdin_lock.tv_sec = 0;	
		else
			_stdin_lock.tv_sec = _timeout.tv_sec - passed;
		timeout = &_stdin_lock;
	}

	for (auto const & p : _data_socks) { //TODO: wydzielic do funkcji
		if (p.cmd == READ) {
			if (p.sent) {
				if (p.file > max)
					max = p.file;
				FD_SET(p.file, &rfds);
				cout << "set reading file" << endl;
			} else {
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

	FD_SET(_udp_sock, &rfds);
	max = max < _udp_sock ? _udp_sock : max;

	if (!_cmd_queue.empty()) {
		FD_SET(_udp_sock, &wfds);
	}
	//NOTE: odrzucenie przeterminowanych. timeout

	int a = select(max + 1, &rfds, &wfds, nullptr, timeout);

	if (a == 0) {
		_locked = false;
	} else {
		if (FD_ISSET(STDIN, &rfds)) {
			char buf[MAX_UDP];
			read(STDIN, buf, MAX_UDP);
			parse_command(buf);
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
			if (FD_ISSET(p.file, &rfds)) {
				cout << "we can read from the file" << endl;
				read_file(p);
			}
			if (FD_ISSET(p.file, &wfds)) {
				cout << "we can write to the file" << endl;
				int a = write_file(p);
				if (a < 0) {
					todel(p);
				}
			}
			if (FD_ISSET(p.socket, &rfds)) {
				recv_file(p);
			}
			if (FD_ISSET(p.socket, &wfds)) {
				cout << "socket is open so im sending!" << endl;
				int a = send_file(p);
				if (a < 0) {
					todel(p);
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
	if ("discover" /= buf) {
		_servers.clear();
		syncronous_command(1);
	} else if ("search" /= buf) {
		string cmd("search");
		char filename[MAX_BUF];
		sscanf(buf.substr(buf.find(cmd) + cmd.size(), buf.size()).c_str(), "%s", filename);
		syncronous_command(2, filename);
	} else if ("fetch" /= buf) {
		cout << "fetching" << endl;
		string cmd("fetch");
		char filename[MAX_BUF];
		sscanf(buf.substr(buf.find(cmd) + cmd.size(), buf.size()).c_str(), "%s", filename);
		if (_listed_filenames.find(filename) != _listed_filenames.end()) {
			_cmd_queue.push(shared_ptr <Command> {new GetCmd{_multicast,
															_cmd_seq,
															filename}});
		} else {
			cout << "nie znaleziono w liscie pliku : " << filename << endl;
		}
	} else if ("upload" /= buf) { //find the file
		string cmd("upload");
		char filename[MAX_BUF];
		sscanf(buf.substr(buf.find(cmd) + cmd.size(), buf.size()).c_str(), "%s", filename);
		uint64_t size = 0;
		AddCmd * cmd = nullptr;
		path file;
		if (!_servers.empty()) { //musi się zmieścić
			sockaddr_in best;
			for (const auto & serv : _servers) {
				if (serv.first > size) {
					best = serv.second
					break;
				}
			}
			if (filename[0] == '/' && is_regular_file(filename)) {
				file = filename;
				cmd = new AddCmd{best, _cmd_seq, size, file.filename()}
			} else if (is_regular_file(_current_dir / filename)) {
				file = _current_dir / filename;
				cmd = new AddCmd{best, _cmd_seq, size, file.filename()}
			}
		}
		// TODO: a potem do następnego który ma miejsce
		if (cmd != nullptr) {
			_cmd_queue.push(shared_ptr <Command> {cmd});
			_add_queue.insert(make_pair(shared_ptr <Command> {cmd}, file));
		}
	} else if ("remove" /= buf) {
		string cmd("remove");
		char filename[MAX_BUF];
		sscanf(buf.substr(buf.find(cmd) + cmd.size(), buf.size()).c_str(), "%s", filename);
		if (strlen(filename) != 0) {
			_cmd_queue.push(shared_ptr <Command> {new DelCmd{_multicast,
															_cmd_seq,
															filename}});
		}
	} else if ("exit" /= buf) {
		_exit = true;
	}
}

void
Client::parse_response(const string & buf, sockaddr_in remote_addr)
{
	if (SimplCmd{buf}._cmd_seq != _cmd_seq)
		return;
	if (!strncmp(buf.c_str(), GOOD_DAY, strlen(GOOD_DAY))) {
		GoodCmd cmd{buf, remote_addr};

		_servers.insert(std::pair <uint64_t, sockaddr_in> {gdcmd.getSizeLeft(), remote_addr});
		cout << "Found "
			<< inet_ntoa(remote_addr.sin_addr)
			<< "("
			<< gdcmd.getMCastAddr()
			<< ") with free space "
			<< gdcmd.getSizeLeft() << endl;
	} else if (!strncmp(buf.c_str(), MY_LIST, strlen(MY_LIST))) {
		MyListCmd cmd{buf, remote_addr};
		std::string filename;
		std::istringstream tokenStream(mlcmd.getFileList());

		while (std::getline(tokenStream, filename))
		{
			_listed_filenames.insert(filename);
			cout << filename << " (" << inet_ntoa(remote_addr.sin_addr) << ")" << endl;
		}
	} else if (!strncmp(buf.c_str(), CONNECT_ME, strlen(CONNECT_ME))) {
		ConnectMeCmd cmd{buf, remote_addr};

		for (const auto & it = _get_queue.begin(); it != _get_queue.end(); it++) {
			if (string{cmd->file_name()}.compare((*it)->file_name())) {

				Socket sock;
				sockaddr_in new_remote;
				remote.sin_addr = remote_addr.sin_addr;
				remote.sin_port = cmd.port();
				sock.cmd = READ;
				sock.file = open(_directory / name).c_str(), O_WRONLY);
				open_tcp_sock(sock, new_remote, 0); //TODO obsluga socketow tcp
				_data_socks.push_back(sock);

				_get_queue.erase(it);
				break;
			}
		}

	} else if (!strncmp(buf.c_str(), NO_WAY, strlen(NO_WAY))) {
		NWCmd cmd{buf, remote_addr}; //TODO: sprobowac z nastepnym
	} else if (!strncmp(buf.c_str(), CAN_ADD, strlen(CAN_ADD))) {
		CanAddCmd cmd{buf, remote_addr};

		for (const auto & it = _add_queue.begin(); it != _add_queue.end(); it++) {
			if (string{cmd->file_name()}.compare((*it).first->file_name())) {
	
				Socket sock;
				sockaddr_in new_remote;
				remote.sin_addr = remote_addr.sin_addr;
				remote.sin_port = cmd.port();
				sock.cmd = WRITE;
				sock.file = open(string((*it).second).c_str(), O_RDONLY);
				open_tcp_sock(sock, new_remote, 0); //TODO obsluga socketow tcp
				_data_socks.push_back(sock);

				_add_queue.erase(it);
				break;
			}
		}

	}
}

void
Client::syncronous_command(int parameter, const string & name)
{
	if (parameter == 1) {
		_cmd_queue.push(shared_ptr <Command> {new HelloCmd{_multicast, _cmd_seq}});
	} else {
		_cmd_queue.push(shared_ptr <Command> {new ListCmd{_multicast, _cmd_seq}});
		_listed_filenames.clear();
	}
	_stdin_lock = _timeout;
	_lock_time = system_clock::now();
	_locked = true;
}

void
Client::open_udp_sock()
{
	sockaddr_in local_address;
	_udp_sock = socket(AF_INET, SOCK_DGRAM, 0);
	socklen_t len = sizeof local_address;

	local_address.sin_family = AF_INET;
	local_address.sin_addr.s_addr = htonl(INADDR_ANY);
	local_address.sin_port = 0;

	if (bind(_udp_sock, (struct sockaddr *) &local_address, sizeof local_address) < 0)
		syserr("bind");
}

void
Client::open_tcp_sock(Socket & sock, sockaddr_in remote_addr, int flag)
{
	sock.sent = true;
	sock.cmd = flag;
	sockaddr_in local_address;
	sock.socket = socket(AF_INET, SOCK_STREAM, 0);
	socklen_t len = sizeof local_address;

	local_address.sin_family = AF_INET;
	local_address.sin_addr.s_addr = htonl(INADDR_ANY);
	local_address.sin_port = 0;

	if (bind(sock.socket, (struct sockaddr *) &local_address, sizeof local_address) < 0)
		syserr("bind");

	connect(sock.socket, (sockaddr *) &remote_addr, sizeof remote_addr);
}

/*
discover - synchronous

	Found 10.1.1.28 (239.10.11.12) with free space 23456

search %s - synchronous

    {nazwa_pliku} ({ip_serwera})
{
fetch %s - async

	File {%s} downloaded ({ip_serwera}:{port_serwera})
	File {%s} downloading failed ({ip_serwera}:{port_serwera}) {opis_błędu}

upload %s - async
	File {%s} does not exist
	File {%s} too big
	File {%s} uploaded ({ip_serwera}:{port_serwera})
	File {%s} uploading failed ({ip_serwera}:{port_serwera}) {opis_błędu}
} - tcp socket packet queue
remove %s - async, not empty

exit
*/
