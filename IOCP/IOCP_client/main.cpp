#include <iostream>
#include <cstdlib>
#include "client.h"

int main(int argc, char* argv[]) {
	if (argc != 4) {
		std::cout << "Usage " << argv[0] << " <ip> <port> <name>" << std::endl;
		exit(1);
	}

	Client clnt(argv[3], argv[1], argv[2]);
	clnt.InitWinSock();
	clnt.MakeClientSocket();
	clnt.ConnectServer();
	clnt.MakeThread();

	return 0;
}