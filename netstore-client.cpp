#include "client.hpp"

int main()
{
	Client client("239.10.11.12", 1337, "tmp", 5);
	
	while (client.run()) {}
}
