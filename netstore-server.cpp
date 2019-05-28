#include "server.hpp"


#include <netinet/in.h>

using namespace std;

void usage(const char * name)
{
	cerr << "Usage: " << name << " <-g mcast_addr> <-p cmd_port> <-f shrd_fldr> [-b max_space] [-t timeout]" << endl;
	exit(1);
}

int main(int argc, char * argv[])
{

	char * mcast_addr = nullptr, * shrd_fldr = nullptr;
	int64_t cmd_port = -1, timeout = 5;
	int64_t max_space = 52428800;
	for (int i = 1; i < argc; i++) {
		if (!strcmp(argv[i], "-g")) {
			mcast_addr = argv[i + 1];
		} else if (!strcmp(argv[i], "-p")) {
			cmd_port = atoi(argv[i + 1]);
		} else if (!strcmp(argv[i], "-f")) {
			shrd_fldr = argv[i + 1];
		} else if (!strcmp(argv[i], "-b")) {
			max_space = atoi(argv[i + 1]);
		} else if (!strcmp(argv[i], "-t")) {
			timeout = atoi(argv[i + 1]);
		}
	}
	if (!mcast_addr || cmd_port < 0 || !shrd_fldr) {
		usage(argv[0]);
	}

	if (timeout > 300)
		timeout = 300;
	if (timeout < 0)
		timeout = 0;

	Server server(mcast_addr, cmd_port, shrd_fldr, max_space, timeout);
// 	Server server(string("239.10.11.12"), (in_port_t)1337, string("build"));
	while (true)
		server.run();
}
