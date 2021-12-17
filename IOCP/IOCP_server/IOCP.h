#ifndef __IOCP_H__
#define __IOCP_H__
#include <WinSock2.h>
#include <thread>

#define BUF_SIZE 1024
#define MAX_CLNT 200
#define MAX_THREAD 100
#define READ 3
#define WRITE 5

typedef struct {
	SOCKET hClntSock;
	SOCKADDR_IN clntAddr;
}CLNT_INFO, * LP_CLNT_INFO;

typedef struct {
	OVERLAPPED overlapped;
	WSABUF wsaBuf;
	char buffer[BUF_SIZE];
	int rwMode;
}IO_INFO, * LP_IO_INFO;

class IOCP {
private:
	WSADATA wsaData;
	HANDLE hComPort;
	SYSTEM_INFO sysInfo;
	LP_IO_INFO ioInfo;
	LP_CLNT_INFO clntInfo;

	SOCKET hServSock;
	SOCKADDR_IN servAddr;
	DWORD dwRecvBytes = 0, dwFlags = 0;
	std::thread* threadArr[MAX_THREAD];
	int port;

public:
	IOCP(const char* port);
	~IOCP();
	void InitWinSock();
	void CreateCompletionPort(HANDLE fileHandle = INVALID_HANDLE_VALUE, HANDLE existingCompletionPort = NULL, ULONG_PTR completionKey = 0, DWORD numberOfConcurrentThreads = MAX_THREAD);
	void MakeThread();
	void MakeServerSocket();
	void SocketBinding() const;
	void SocketListening() const;
	void AcceptRoutine();
	void ErrorHandling(const char* msg) const;
	IOCP(const IOCP& ref); // 복사 생성자 정의(새로운 객체를 생성할 때)
	IOCP& operator=(const IOCP& ref); // 대입 연산자 오버로딩(이미 생성되어 있는 객체에 대입할 때)
};

void EchoThreadMain(HANDLE hComPort);
#endif