#ifndef __CLIENT_H__
#define __CLIENT_H__

#include <WinSock2.h>
#include <thread>

#define BUF_SIZE 100
#define NAME_SIZE 20

class Client {
private:
	WSADATA wsaData;
	SOCKET hSock;
	SOCKADDR_IN servAddr;
	std::thread* hSendThread;
	std::thread* hRecvThread;
	char ip[BUF_SIZE];
	int port;

public:
	Client(const char* name, const char* ip, const char* port);
	~Client();
	void InitWinSock();
	void ErrorHandling(const char* msg) const;
	void MakeClientSocket();
	void ConnectServer();
	void MakeThread();
	Client(const Client& ref);
	Client& operator=(const Client& ref);
};

void SendMsg(SOCKET hSock);
void RecvMsg(SOCKET hSock);
#endif