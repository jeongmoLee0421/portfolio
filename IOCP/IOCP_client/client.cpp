#include "client.h"
#include <iostream>
#include <cstdlib>
#include <cstring>
#include <Windows.h>
#include <process.h>
#pragma warning (disable:4996)

namespace CLNT {
	char name[NAME_SIZE] = "[DEFAULT]";
	char msg[BUF_SIZE];
}

// string = printf
// argv[3]을 printf 출력형식으로 name에 저장
// argv[1]을 ip에 복사, argv[2]를 port에 정수로 형변환해서 저장
Client::Client(const char* name, const char* ip, const char* port):port(atoi(port)) {
	sprintf(CLNT::name, "[%s]", name);
	strcpy(this->ip, ip);
}

// 스레드 메모리 해제, 클라이언트 소켓 닫기, winsock 리소스 반환
Client::~Client() {
	delete hSendThread;
	delete hRecvThread;

	closesocket(hSock);
	WSACleanup();
}

// winsock 초기화
void Client::InitWinSock(){
	if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0)
		ErrorHandling("WSAStartup() error");
}

// 에러 처리
void Client::ErrorHandling(const char* msg) const {
	fputs(msg, stderr);
	fputc('\n', stderr);
	exit(1);
}

// client 소켓을 생성하고 접속할 서버 구조체를 초기화
void Client::MakeClientSocket() {
	hSock = socket(PF_INET, SOCK_STREAM, 0);
	if (hSock == INVALID_SOCKET)
		ErrorHandling("socket() error");

	memset(&servAddr, 0, sizeof(servAddr));
	servAddr.sin_family = AF_INET;
	servAddr.sin_addr.s_addr = inet_addr(ip);
	servAddr.sin_port = htons(port);
}

// 서버에 연결 요청
void Client::ConnectServer() {
	if (connect(hSock, (SOCKADDR*)&servAddr, sizeof(servAddr)) == SOCKET_ERROR)
		ErrorHandling("connect() error");

	std::cout << "connect..." << std::endl;
}

// 스레드 생성
void Client::MakeThread() {
	// send, recv 처리 스레드 생성
	hSendThread = new std::thread(SendMsg, hSock);
	hRecvThread = new std::thread(RecvMsg, hSock);

	// 자식 스레드가 종료될 때까지 대기
	hSendThread->join();
	hRecvThread->join();
}

// 메시지 송신
void SendMsg(SOCKET hSock) {
	char nameMsg[NAME_SIZE + BUF_SIZE];

	// q 또는 Q가 입력되면 소켓 닫고 종료
	// 이외의 메시지가 입력되면 소켓을 사용해 서버로 송신
	while (1) {
		fgets(CLNT::msg, BUF_SIZE, stdin);
		if (!strcmp(CLNT::msg, "q\n") || !strcmp(CLNT::msg, "Q\n")) {
			closesocket(hSock);
			exit(0);
		}
		sprintf(nameMsg, "%s %s", CLNT::name, CLNT::msg);
		send(hSock, nameMsg, strlen(nameMsg), 0);
	}
	return;
}

// 메시지 수신
void RecvMsg(SOCKET hSock) {
	char nameMsg[NAME_SIZE + BUF_SIZE];
	int strLen;

	// 수신한 메시지 콘솔에 출력
	while (1) {
		strLen = recv(hSock, nameMsg, NAME_SIZE + BUF_SIZE - 1, 0);
		if (strLen == -1)
			return;

		nameMsg[strLen] = 0;
		fputs(nameMsg, stdout);
	}
	return;
}

Client::Client(const Client& ref) {
	wsaData = ref.wsaData;
	hSock = ref.hSock;
	servAddr = ref.servAddr;

	//스레드는 복사 안함

	strcpy(ip, ref.ip);
	port = ref.port;
}

Client& Client::operator=(const Client& ref) {
	// 동적 할당한 메모리가 없기 때문에 그냥 복사해도 무방

	wsaData = ref.wsaData;
	hSock = ref.hSock;
	servAddr = ref.servAddr;

	//스레드는 복사 안함

	strcpy(ip, ref.ip);
	port = ref.port;

	return *this;
}