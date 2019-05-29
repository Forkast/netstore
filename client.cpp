#include "client.hpp"
#include "protocol.hpp"

#include <unistd.h>

#define STDIN 0

Client::Client(const string & mcast_addr, in_port_t cmd_port, const string & directory, int timeout)
	: _mcast_addr{mcast_addr},
	  _cmd_port{cmd_port},
	  _directory{directory},
	  _timeout{timeout, 0}
{
	
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
		}
	}
}
