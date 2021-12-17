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
// argv[3]�� printf ����������� name�� ����
// argv[1]�� ip�� ����, argv[2]�� port�� ������ ����ȯ�ؼ� ����
Client::Client(const char* name, const char* ip, const char* port):port(atoi(port)) {
	sprintf(CLNT::name, "[%s]", name);
	strcpy(this->ip, ip);
}

// ������ �޸� ����, Ŭ���̾�Ʈ ���� �ݱ�, winsock ���ҽ� ��ȯ
Client::~Client() {
	delete hSendThread;
	delete hRecvThread;

	closesocket(hSock);
	WSACleanup();
}

// winsock �ʱ�ȭ
void Client::InitWinSock(){
	if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0)
		ErrorHandling("WSAStartup() error");
}

// ���� ó��
void Client::ErrorHandling(const char* msg) const {
	fputs(msg, stderr);
	fputc('\n', stderr);
	exit(1);
}

// client ������ �����ϰ� ������ ���� ����ü�� �ʱ�ȭ
void Client::MakeClientSocket() {
	hSock = socket(PF_INET, SOCK_STREAM, 0);
	if (hSock == INVALID_SOCKET)
		ErrorHandling("socket() error");

	memset(&servAddr, 0, sizeof(servAddr));
	servAddr.sin_family = AF_INET;
	servAddr.sin_addr.s_addr = inet_addr(ip);
	servAddr.sin_port = htons(port);
}

// ������ ���� ��û
void Client::ConnectServer() {
	if (connect(hSock, (SOCKADDR*)&servAddr, sizeof(servAddr)) == SOCKET_ERROR)
		ErrorHandling("connect() error");

	std::cout << "connect..." << std::endl;
}

// ������ ����
void Client::MakeThread() {
	// send, recv ó�� ������ ����
	hSendThread = new std::thread(SendMsg, hSock);
	hRecvThread = new std::thread(RecvMsg, hSock);

	// �ڽ� �����尡 ����� ������ ���
	hSendThread->join();
	hRecvThread->join();
}

// �޽��� �۽�
void SendMsg(SOCKET hSock) {
	char nameMsg[NAME_SIZE + BUF_SIZE];

	// q �Ǵ� Q�� �ԷµǸ� ���� �ݰ� ����
	// �̿��� �޽����� �ԷµǸ� ������ ����� ������ �۽�
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

// �޽��� ����
void RecvMsg(SOCKET hSock) {
	char nameMsg[NAME_SIZE + BUF_SIZE];
	int strLen;

	// ������ �޽��� �ֿܼ� ���
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

	//������� ���� ����

	strcpy(ip, ref.ip);
	port = ref.port;
}

Client& Client::operator=(const Client& ref) {
	// ���� �Ҵ��� �޸𸮰� ���� ������ �׳� �����ص� ����

	wsaData = ref.wsaData;
	hSock = ref.hSock;
	servAddr = ref.servAddr;

	//������� ���� ����

	strcpy(ip, ref.ip);
	port = ref.port;

	return *this;
}