#include "server.hpp"


#include <arpa/inet.h>
#include <errno.h>
#include <memory>
#include <string.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <iostream>
#include <unistd.h>

#define syserr(x) {cerr << "Error making " << x << ". Code " << errno << " : " << strerror(errno) << endl; \
exit(0);}

using namespace std;
using namespace std::filesystem;

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
	fd_set rfds, wfds;;

	FD_ZERO(&rfds);
	FD_SET(_sock, &rfds);
	int max = _sock;
	timeval timeout = _timeout;

	FD_ZERO(&wfds);
	for (auto const & p : _data_socks) {
		if (p.socket > max)
			max = p.socket;
		FD_SET(p.socket, &wfds);
		if (p.timeout.tv_sec < timeout.tv_sec) // TODO: dodac milisekundy
			timeout = p.timeout;
	}

	int a = select(max + 1, &rfds, &wfds, nullptr, &timeout);
	cout << "select out\n";

	if (a == 0) {
		//for (//wywalic przeterminowane sockety
	} else {
		if (FD_ISSET(_sock, &rfds)) {
			sockaddr remote_addr;
			socklen_t socklen;
			char buf[Command::MAX];
			int len = recvfrom(_sock, buf, Command::MAX, 0, &remote_addr, &socklen); //recvfrom
			cout << "przeczytalem " << len << " bajtow" << endl;
			auto cmd = shared_ptr<Command>(get_command(string(buf)));
			getCmdSeq(cmd);
			cmd->getCmd();
		}
		for (auto const & p : _data_socks) {
			if (FD_ISSET(p.socket, &wfds)) {
				send_file(p);
			}
		}
	}
}

void
Server::index_files()
{
	for (directory_entry const & f : directory_iterator(_directory)) {
		if (f.is_regular_file()) {
// 			_files.insert(f);
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
	cout << _sock << endl;
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

Command *
Server::get_command(const string & buf)
{
	if (!buf.compare(HELLO)) {
		return new HelloCmd{};
	} else if (!buf.compare(LIST)) {
		return new ListCmd{};
	} else if (!buf.compare(GET)) {
		return new GetCmd{};
	} else if (!buf.compare(DEL)) {
		return new DelCmd{};
	} else if (!buf.compare(ADD)) {
		return new AddCmd{};
	} else {
		return nullptr;
	}
}

void
Server::getCmdSeq(shared_ptr<Command> &cmd)
{
	sockaddr remote_addr;
	socklen_t socklen;
	uint64_t seq;
	recvfrom(_sock, &seq, sizeof seq, 0, &remote_addr, &socklen);
	cout << hex << seq << endl;
	cmd->setNetworkSeq(seq);
}

/* Jeśli serwer otrzyma polecenie dodania pliku lub pobrania pliku, to powinien otworzyć nowe gniazdo TCP na losowym wolnym porcie przydzielonym przez system operacyjny i port ten przekazać w odpowiedzi węzłowi klienckiemu. Serwer oczekuje maksymalnie TIMEOUT sekund na nawiązanie połączenia przez klienta i jeśli takie nie nastąpi, to port TCP powinien zostać niezwłocznie zamknięty. Serwer w czasie oczekiwania na podłączenie się klienta i podczas przesyłania pliku powinien obsługiwać także inne zapytania od klientów.*/
void
Server::read_and_parse()
{
	// commands: Hello -> GoodDay, List -> MyList, Get -> Connect_me, Del -> null, Add -> no_way / can_add
	
}

void
Server::send_file(Socket socket)
{
	
}

/* non blocking connect with timeout
int main(int argc, char **argv) {
    u_short port;                /* user specified port number
    char *addr;                  /* will be a pointer to the address
    struct sockaddr_in address;  /* the libc network address data structure
    short int sock = -1;         /* file descriptor for the network socket
    fd_set fdset;
    struct timeval tv;

    if (argc != 3) {
        fprintf(stderr, "Usage %s <port_num> <address>\n", argv[0]);
        return EXIT_FAILURE;
    }

    port = atoi(argv[1]);
    addr = argv[2];

    address.sin_family = AF_INET;
    address.sin_addr.s_addr = inet_addr(addr); /* assign the address
    address.sin_port = htons(port);            /* translate int2port num

    sock = socket(AF_INET, SOCK_STREAM, 0);
    fcntl(sock, F_SETFL, O_NONBLOCK);

    connect(sock, (struct sockaddr *)&address, sizeof(address));

    FD_ZERO(&fdset);
    FD_SET(sock, &fdset);
    tv.tv_sec = 10;             /* 10 second timeout
    tv.tv_usec = 0;

    if (select(sock + 1, NULL, &fdset, NULL, &tv) == 1)
    {
        int so_error;
        socklen_t len = sizeof so_error;

        getsockopt(sock, SOL_SOCKET, SO_ERROR, &so_error, &len);

        if (so_error == 0) {
            printf("%s:%d is open\n", addr, port);
        }
    }

    close(sock);
    return 0;
} */
