#include "client.hpp"

int main()
{
	Client client("", 1337, "tmp", 5);
	
	while (true)
		client.run();
}
