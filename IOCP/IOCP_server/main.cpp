#include <iostream>
#include <cstdlib>
#include "IOCP.h"

int main(int argc, char* argv[]) {
	if (argc != 2) {
		std::cout << "Usage " << argv[0] << " <port>" << std::endl;
		exit(1);
	}

	IOCP iocp(argv[1]);
	iocp.InitWinSock();
	iocp.CreateCompletionPort();
	iocp.MakeThread();
	iocp.MakeServerSocket();
	iocp.SocketBinding();
	iocp.SocketListening();
	iocp.AcceptRoutine();

	return 0;
}