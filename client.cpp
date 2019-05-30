#include "client.hpp"
#include "protocol.hpp"

#include <cctype>
#include <unistd.h>
#include <sys/socket.h>

using namespace std;

#define STDIN 0

//TODO: walidacja cmd_seq
//TODO: double discover

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
	open_udp_sock();
}

void
Client::run()
{
	fd_set rfds, wfds;

	FD_ZERO(&rfds);
	FD_ZERO(&wfds);
	FD_SET(STDIN, &rfds);
	int max = STDIN;
	timeval timeout = _timeout;

	int a = select(max + 1, &rfds, &wfds, nullptr, &timeout);

	if (a == 0) {
		
	} else {
		if (FD_ISSET(STDIN, &rfds)) {
			char buf[MAX_UDP];
			read(STDIN, buf, MAX_UDP);
			parse_command(buf);
		}
	}
}

void
Client::parse_command(const string & buf)
{
	if ("discover" /= buf) {
		costamzrob(1, string());
	} else if ("search" /= buf) {
		costamzrob(2, string());
	} else if ("fetch" /= buf) {
		
	} else if ("upload" /= buf) {
		
	} else if ("remove" /= buf) {
		
	} else if ("exit" /= buf) {
		
	}
}

void
Client::costamzrob(int parameter, const string & name)
{
	shared_ptr <Command> cmd;
	if (parameter == 1) {
		cmd = shared_ptr <Command> (new HelloCmd{_multicast, _cmd_seq});
	} else {
		cmd = shared_ptr <Command> (new ListCmd{_multicast, _cmd_seq});
	}
	cmd->send(_udp_sock);
	fd_set rfds;
	FD_ZERO(&rfds);
	FD_SET(_udp_sock, &rfds);
	timeval timeout = _timeout;
	chrono::system_clock::time_point start_time = chrono::time_point_cast<chrono::seconds> (chrono::system_clock::now());

	int a = 1;
	while (a != 0) {
		auto passed = chrono::time_point_cast<chrono::seconds> (chrono::system_clock::now()) - start_time;
		timeout.tv_sec -= passed.count() > timeout.tv_sec ? timeout.tv_sec : passed.count();
		a = select(_udp_sock + 1, &rfds, nullptr, nullptr, &timeout);
		if (a != 0) {
			if (FD_ISSET(_udp_sock, &rfds)) {
				sockaddr_in remote_addr;
				socklen_t len = sizeof remote_addr;
				char buf[MAX_UDP];
				int a = recvfrom(_udp_sock, buf, MAX_UDP, 0, (sockaddr *) &remote_addr, &len);
				cout << "przeczytano " << a << " bajtow" << endl;
				string s(buf, a);
				if (parameter == 1) {
					GoodDayCmd gdcmd{s, remote_addr};
					cout << "Found "
						<< inet_ntoa(remote_addr.sin_addr)
						<< "("
						<< gdcmd.getMCastAddr()
						<< ") with free space "
						<< gdcmd.getSizeLeft() << endl;
				} else {
					MyListCmd mlcmd{s, remote_addr};
					cout << mlcmd.getFileList() << endl;
				}
			}
		}
	}
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

/* https://stackoverflow.com/questions/11635/case-insensitive-string-comparison-in-c
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
