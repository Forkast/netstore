#include "server.hpp"


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
	FD_SET(_sock, &rfds);
	int max = _sock;
	timeval timeout = _timeout;

	FD_ZERO(&wfds);
	for (auto const & p : _data_socks) {
		if (p.sent) {
			if (p.file > max)
				max = p.file;
			FD_SET(p.file, &rfds);
			cout << "set reading socket" << endl;
		} else {// czytam tylko jak poprzednia paczka została wysłana
			sockaddr addr;
			socklen_t len = sizeof addr;
			if (getpeername(p.socket, &addr, &len) == 0) {
				if (p.socket > max)
					max = p.socket;
				FD_SET(p.socket, &wfds);
				cout << "set writing tcp socket" << endl;
			} // TODO: usuwamy po timeoucie
		}
// 		if (p.timeout.tv_sec < timeout.tv_sec) // TODO: dodac milisekundy
// 			timeout = p.timeout;
	}

	if (!_cmd_queue.empty()) {
		cout << "is not empty" << endl;
		FD_SET(_sock, &wfds);
	}

	int a = select(max + 1, &rfds, &wfds, nullptr, &timeout);
	cout << "select out\n";

	if (a == 0) {
		//for (//wywalic przeterminowane sockety
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
			if (FD_ISSET(p.socket, &wfds)) {
				cout << "socket is open so im sending!" << endl;
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
		uint64_t space_left;
		if (_space_used > _max_space)
			space_left = 0;
		else
			space_left = _max_space - _space_used;
		_cmd_queue.push(shared_ptr <Command> {new GoodDayCmd{buf,
															  remote_addr,
															  _mcast_addr,
															  space_left}});
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
		sock.sent = true;
		sock.start_time = std::chrono::system_clock::now();
		open_tcp_port(sock);
		_data_socks.push_back(sock);
		_cmd_queue.push(shared_ptr <Command> (new ConnectMeCmd{buf,
													remote_addr,
													string(get_cmd.file_name()),
													(uint64_t)sock.socket}));
	} else if (!strncmp(buf.c_str(), DEL, strlen(DEL))) {
		
	} else if (!strncmp(buf.c_str(), ADD, strlen(ADD))) {
		
	}
}

void
Server::open_tcp_port(Socket & sock)
{
	sockaddr_in local_address;
	sock.socket = socket(AF_INET, SOCK_STREAM, 0);
	socklen_t len = sizeof local_address;

	local_address.sin_family = AF_INET;
	local_address.sin_addr.s_addr = INADDR_ANY;
	local_address.sin_port = 0;

	if (bind(sock.socket, (struct sockaddr *) &local_address, sizeof local_address) < 0)
		syserr("bind");

	if (getsockname(sock.socket, (struct sockaddr *)&local_address, &len) < 0)
		syserr("getsockname");

	fcntl(sock.socket, F_SETFL, O_NONBLOCK);
	cout << "nowy port to " << local_address.sin_port << endl;

	connect(sock.socket, (struct sockaddr *)&local_address, sizeof local_address);
}

/* Jeśli serwer otrzyma polecenie dodania pliku lub pobrania pliku, to powinien otworzyć nowe gniazdo TCP na losowym wolnym porcie przydzielonym przez system operacyjny i port ten przekazać w odpowiedzi węzłowi klienckiemu. Serwer oczekuje maksymalnie TIMEOUT sekund na nawiązanie połączenia przez klienta i jeśli takie nie nastąpi, to port TCP powinien zostać niezwłocznie zamknięty. Serwer w czasie oczekiwania na podłączenie się klienta i podczas przesyłania pliku powinien obsługiwać także inne zapytania od klientów.*/
void
Server::read_and_parse()
{
	// commands: Hello -> GoodDay, List -> MyList, Get -> Connect_me, Del -> null, Add -> no_way / can_add
	
}

void
Server::send_file(Socket & sock)
{
	int a = send(sock.socket, sock.buf, sock.size, 0);
	if (a < 0)
		prnterr("sending file");
	sock.sent = true;
	sock.size = 0;
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
	for (auto f : _files) {
		if (!f.compare(name)) {
			cout << "opening: " << _directory + "/" + f << endl;
			return open((_directory + "/" + f).c_str(), flags);
		}
	}
	return -1;
}

inline int64_t maks(int64_t a, int64_t b)
{
	return a > b ? a : b;
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
