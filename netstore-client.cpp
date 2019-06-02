#include "client.hpp"

void usage(const char * name)
{
	cerr << "Usage: " << name << " <-g mcast_addr> <-p cmd_port> <-f shrd_fldr> [-b max_space] [-t timeout]" << endl;
	exit(1);
}

int main(int argc, char * argv[])
{

	char * mcast_addr = nullptr, * out_fldr = nullptr;
	int64_t cmd_port = -1, timeout = 5;
	int64_t max_space = 52428800;
	for (int i = 1; i < argc; i++) {
		if (!strcmp(argv[i], "-g")) {
			mcast_addr = argv[i + 1];
		} else if (!strcmp(argv[i], "-p")) {
			cmd_port = atoi(argv[i + 1]);
		} else if (!strcmp(argv[i], "-o")) {
			out_fldr = argv[i + 1];
		} else if (!strcmp(argv[i], "-t")) {
			timeout = atoi(argv[i + 1]);
		}
	}
	if (!mcast_addr || cmd_port < 0 || !out_fldr) {
		usage(argv[0]);
	}

	if (timeout > 300)
		timeout = 300;
	if (timeout < 0)
		timeout = 0;

	Client client(mcast_addr, cmd_port, out_fldr, timeout);

	while (client.run()) {}
}
