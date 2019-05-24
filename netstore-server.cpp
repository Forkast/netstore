#include "server.hpp"


#include <netinet/in.h>

using namespace std;

int main()
{
	Server server(string("239.10.11.12"), (in_port_t)1337, string("build"));
	while (true)
		server.run();
}
