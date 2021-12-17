#include "IOCP.h"
#include <cstring>
#include <iostream>
#pragma warning (disable:4996)

// CLNT �̸������� ����� �����ϸ� ���� �� ���ǵ� ��ȣ�� �ֽ��ϴ�/obj�� �̹� ���ǵǾ� �ֽ��ϴ� ���� �߻�
// main.cpp�� IOCP.cpp���� iocp.h�� ���ÿ� �����ؼ� �߻��ϴ� ����
// namespace�� �̹� ����Ǿ� �ִ°��̰� cpp���Ͽ����� ����ϹǷ� h���Ͽ��� cpp���Ϸ� �ű�
namespace CLNT {
	int numOfClnt;
	SOCKET clntArr[MAX_CLNT];
}

// ���������ڰ� ������ ��Ʈ�� ���ڿ��� �޾� ���������� ����
IOCP::IOCP(const char* port) {
	this->port = atoi(port);
}

// ���� �Ҵ��� ������ �޸� ����
IOCP::~IOCP() {
	for (DWORD i = 0; i < sysInfo.dwNumberOfProcessors * 2; i++)
		delete threadArr[i];
}

// winsock �ʱ�ȭ
void IOCP::InitWinSock() {
	if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0)
		ErrorHandling("WSAStartup() error");
}

// ����� completion port ������Ʈ ����(���Ŀ� ������ �� ������Ʈ�� ����)
// ������ ���ڿ� 0�� �ѱ�� �ý����� ���μ���(�ھ�)��ŭ cp ������Ʈ�� ������ ������ �� ���
void IOCP::CreateCompletionPort(HANDLE fileHandle, HANDLE existingCompletionPort, ULONG_PTR completionKey, DWORD numberOfConcurrentThreads) {
	hComPort = CreateIoCompletionPort(fileHandle, existingCompletionPort, completionKey, numberOfConcurrentThreads);
}

// �� CPU�� �ھ�� 6���� �� 12���� �����尡 �����Ǿ� hComPort��ü�� ����
void IOCP::MakeThread(){
	GetSystemInfo(&sysInfo);
	
	// c++���� ������ ������ �� join�� ����ؼ� �����尡 �����ϴ� ���� Ȯ���ϰڴٰ� ����ؾ���
	// ������� ������ abort() has been called ���� �߻�
	for (DWORD i = 0; i < sysInfo.dwNumberOfProcessors * 2; i++)
		threadArr[i] = new std::thread(EchoThreadMain, hComPort);
}

// overlapped�� ������ ���� ���� ���� �� �����ּ� ����ü�� ipv4, �ý��� ip�ּ�, ��Ʈ��ȣ �ʱ�ȭ
void IOCP::MakeServerSocket() {
	hServSock = WSASocket(PF_INET, SOCK_STREAM, 0, NULL, 0, WSA_FLAG_OVERLAPPED);
	if (hServSock == INVALID_SOCKET)
		ErrorHandling("WSASocket() error");

	std::memset(&servAddr, 0, sizeof(servAddr));
	servAddr.sin_family = AF_INET;
	servAddr.sin_addr.s_addr = htonl(INADDR_ANY);
	servAddr.sin_port = htons(port);
}

// �������Ͽ� ������ ip�ּҿ� ��Ʈ��ȣ�� ���ε�(�ü���� ���� ������ ����صξ�� Ŭ���̾�Ʈ�� �۽��� ��Ŷ�� ������ �� ����)
void IOCP::SocketBinding() const {
	if (bind(hServSock, (SOCKADDR*)&servAddr, sizeof(servAddr)) == SOCKET_ERROR)
		ErrorHandling("bind() error");

	std::cout << "binding..." << std::endl;
}

// ������ ���Ż��·� �����ν� Ŭ���̾�Ʈ�� ���� ��û�� ����(��� ť�� ũ��� 5)
void IOCP::SocketListening() const {
	if (listen(hServSock, 5) == SOCKET_ERROR)
		ErrorHandling("listen() error");

	std::cout << "listening..." << std::endl;
}

// ���� ��û �޴� ��ƾ
void IOCP::AcceptRoutine() {
	while (true) {
		SOCKET hClntSock;
		SOCKADDR_IN clntAddr;
		int clntAddrSize = sizeof(clntAddr);

		// Ŭ���̾�Ʈ�� �����û�� �����ϰ� ����ϱ� ���� ���� ���� ����
		hClntSock = accept(hServSock, (SOCKADDR*)&clntAddr, &clntAddrSize);
		if (hClntSock == INVALID_SOCKET)
			ErrorHandling("accept() error");

		// Ŭ���̾�Ʈ ���� �߰�
		CLNT::clntArr[CLNT::numOfClnt++] = hClntSock;

		// Ŭ���̾�Ʈ ����(��ȣ)�� �ּҸ� ������ clntInfo ����ü �ʱ�ȭ
		clntInfo = new CLNT_INFO;
		clntInfo->hClntSock = hClntSock;
		memcpy(&(clntInfo->clntAddr), &clntAddr, clntAddrSize);

		// completion port Ŀ�� ������Ʈ�� Ŭ���̾�Ʈ ���� ����
		CreateIoCompletionPort((HANDLE)hClntSock, hComPort, (DWORD)clntInfo, 0);

		// ������ �ۼ��ſ� ���� ������ ������ ioInfo ����ü �ʱ�ȭ
		ioInfo = new IO_INFO;
		memset(&(ioInfo->overlapped), 0, sizeof(OVERLAPPED));
		ioInfo->wsaBuf.len = BUF_SIZE;
		ioInfo->wsaBuf.buf = ioInfo->buffer;
		ioInfo->rwMode = READ;

		// Ŭ���̾�Ʈ�κ��� ���� �޽����� ���� ����
		// 6��° ���ڷ� ioInfo ����ü�� overlapped �ּҸ� �����ϰ� �Ǹ� ioInfo ����ü�� �ٸ� �����鵵 ���� �����ϴ�.
		// �ֳ��ϸ� ioInfo ����ü�� �ּҰ� �� ����ü�� ù��° ������ overlapped �ּҿ� �����ϱ� ����
		WSARecv(clntInfo->hClntSock, &(ioInfo->wsaBuf), 1, &dwRecvBytes, &dwFlags, &(ioInfo->overlapped), NULL);
	}

	// ������ �����尡 ����� �� ���� ���� ������(main thread)�� ���ŷ��Ų��.
	// �����尡 ����Ǹ� join()�Լ��� ��ȯ�ǰ� ��� �����尡 ����Ǹ� ��� join()�Լ��� ��ȯ�Ǿ� main thread�� ��� ����
	for (DWORD i = 0; i < sysInfo.dwNumberOfProcessors * 2; i++)
		threadArr[i]->join();
}

// �����尡 �����ϴ� �Լ��� �� ��� �Լ��ų� �����Լ����� �ϱ� ������ �����Լ� �Ǵ� static���� �����Լ��� ������ �Ѵ�.
// ����/static �Լ��� ��ü ���� ���� �̹� ���ǵǱ� ������ ��� ���� ������ �� �� ����.
// �ذ�å: �̸�����(namespace)�� �����ؼ� ��������
void EchoThreadMain(HANDLE hComPort) {
	SOCKET sock;
	DWORD bytesTrans;
	LP_CLNT_INFO clntInfo;
	LP_IO_INFO ioInfo;
	DWORD flags = 0;
	char tempBuf[BUF_SIZE];

	while (true) {
		// IO Completion Queue�� �Ϸ����� �׸��� �����ϸ� GetQueuedCompletionStatus �Լ��� ��ȯ�Ѵ�.
		// ��ȯ�ϸ鼭 3��° ����(clntInfo)���� CreateIoCompletionPort�Լ��� 3��° ����(clntInfo)�� ���޵ǰ�
		// 4��°����(ioInfo)���� WSARecv �Ǵ� WSASend�� 6��° ����&(ioInfo->overlapped)�� ���޵ȴ�.
		GetQueuedCompletionStatus(hComPort, &bytesTrans, (LPDWORD)&clntInfo, (LPOVERLAPPED*)&ioInfo, INFINITE);

		// clntInfo ����ü���� Ŭ���̾�Ʈ ���� ���� ������
		sock = clntInfo->hClntSock;

		// �������� �����̶��
		if (ioInfo->rwMode == READ) {
			std::cout << "message received" << std::endl;

			// bytesTrans�� 0�̶�� ���� EOF�� ���۵� ��
			if (bytesTrans == 0) {
				// ������ �ݰ� �����Ҵ��� �޸�(Ŭ���̾�Ʈ ����, IO ����)�� ����
				closesocket(sock);
				delete clntInfo;
				delete ioInfo;

				// ���� ������ ������ Ŭ���̾�Ʈ�� �޽����� ���� �ʿ䰡 ���� ������ ���� ����
				for (int i = 0; i < CLNT::numOfClnt; i++) {
					if (CLNT::clntArr[i] == sock) {
						for (int j = i; j < CLNT::numOfClnt; j++) {
							CLNT::clntArr[j] = CLNT::clntArr[j + 1];
						}
						break;
					}
				}

				CLNT::numOfClnt--;
				continue;
			}

			// bytesTrans�� 0�� �ƴ϶�� � �޽����� ���۵� ��
			// ������ �����͸� echo �ϱ� ���� �ӽ� ���ۿ� ���� �� ioInfo �޸� ����
			// clntInfo�� ����ؼ� ����ؾ� �ϱ� ������ �޸� ���� ���� ����
			strcpy(tempBuf, ioInfo->buffer);
			delete ioInfo;

			// ������ ������ ��� Ŭ���̾�Ʈ���� ä�� ����
			// �� ��¸��� ���ο� OVERLAPPED ����ü ���� �Ҵ�
			for (int i = 0; i < CLNT::numOfClnt; i++) {
				ioInfo = new IO_INFO;
				memset(&(ioInfo->overlapped), 0, sizeof(OVERLAPPED));
				ioInfo->wsaBuf.len = bytesTrans;
				ioInfo->wsaBuf.buf = tempBuf;
				ioInfo->rwMode = WRITE;
				WSASend(CLNT::clntArr[i], &(ioInfo->wsaBuf), 1, NULL, 0, &(ioInfo->overlapped), NULL);
			}

			// ä���� ���������� �� �ٸ� ä���� ���� �� �־����
			ioInfo = new IO_INFO;
			memset(&(ioInfo->overlapped), 0, sizeof(OVERLAPPED));
			ioInfo->wsaBuf.len = BUF_SIZE;
			ioInfo->wsaBuf.buf = ioInfo->buffer;
			ioInfo->rwMode = READ;
			WSARecv(sock, &(ioInfo->wsaBuf), 1, NULL, &flags, &(ioInfo->overlapped), NULL);
		}
		else {
			// ����� �Ϸ�Ǹ� IO������ �޸� �����Ѵ�.
			std::cout << "message sent" << std::endl;
			delete ioInfo;
		}
	}
	return;
}

void IOCP::ErrorHandling(const char* msg) const {
	fputs(msg, stderr);
	fputc('\n', stderr);
	exit(1);
}

// ���� �����ڸ� ��������� ������ �⺻ ���Ǵ� ������
IOCP::IOCP(const IOCP& ref) {
	wsaData = ref.wsaData;
	hComPort = ref.hComPort;
	sysInfo = ref.sysInfo;
	
	// ���� �Ҵ��� ��� ���� �޸𸮸� ����Ű�� �ʰ� ���� �Ҵ��ؼ� ����
	ioInfo = new IO_INFO;
	if (ref.ioInfo != NULL) {
		ioInfo->overlapped = ref.ioInfo->overlapped;
		ioInfo->wsaBuf.buf = ioInfo->buffer;
		ioInfo->wsaBuf.len = BUF_SIZE;
		strcpy(ioInfo->buffer, ref.ioInfo->buffer);
		ioInfo->rwMode = ref.ioInfo->rwMode;
	}

	clntInfo = new CLNT_INFO;
	if (ref.clntInfo != NULL) {
		clntInfo->hClntSock = ref.clntInfo->hClntSock;
		clntInfo->clntAddr = ref.clntInfo->clntAddr;
	}

	dwRecvBytes = ref.dwRecvBytes;
	dwFlags = ref.dwFlags;

	//������� �������� ���� ����

	port = ref.port;
}

// ���� �����ڸ� ��������� ������ �����ε� �ص���
IOCP& IOCP::operator=(const IOCP& ref) {
	// �޸� ������ �����ϱ� ���� �����Ҵ� ���� �޸� �̸� ����
	if (ioInfo != NULL)
		delete ioInfo;
	if (clntInfo != NULL)
		delete clntInfo;

	wsaData = ref.wsaData;
	hComPort = ref.hComPort;
	sysInfo = ref.sysInfo;

	ioInfo = new IO_INFO;
	if (ref.ioInfo != NULL) {
		ioInfo->overlapped = ref.ioInfo->overlapped;
		ioInfo->wsaBuf.buf = ioInfo->buffer;
		ioInfo->wsaBuf.len = BUF_SIZE;
		strcpy(ioInfo->buffer, ref.ioInfo->buffer);
		ioInfo->rwMode = ref.ioInfo->rwMode;
	}

	clntInfo = new CLNT_INFO;
	if (ref.clntInfo != NULL) {
		clntInfo->hClntSock = ref.clntInfo->hClntSock;
		clntInfo->clntAddr = ref.clntInfo->clntAddr;
	}

	dwRecvBytes = ref.dwRecvBytes;
	dwFlags = ref.dwFlags;

	//������� �������� ���� ����

	port = ref.port;

	return *this; // ��ü �ڽ��� �ǹ��ϴ� �����ڸ� ��ȯ�ϸ� ���๮���� �������� ������ ����
}