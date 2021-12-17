#include "IOCP.h"
#include <cstring>
#include <iostream>
#pragma warning (disable:4996)

// CLNT 이름공간을 헤더에 정의하면 여러 번 정의된 기호가 있습니다/obj에 이미 정의되어 있습니다 오류 발생
// main.cpp과 IOCP.cpp에서 iocp.h에 동시에 접근해서 발생하는 오류
// namespace는 이미 선언되어 있는것이고 cpp파일에서만 사용하므로 h파일에서 cpp파일로 옮김
namespace CLNT {
	int numOfClnt;
	SOCKET clntArr[MAX_CLNT];
}

// 서버관리자가 개방한 포트를 문자열로 받아 정수형으로 변경
IOCP::IOCP(const char* port) {
	this->port = atoi(port);
}

// 동적 할당한 스레드 메모리 해제
IOCP::~IOCP() {
	for (DWORD i = 0; i < sysInfo.dwNumberOfProcessors * 2; i++)
		delete threadArr[i];
}

// winsock 초기화
void IOCP::InitWinSock() {
	if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0)
		ErrorHandling("WSAStartup() error");
}

// 사용할 completion port 오브젝트 생성(이후에 소켓을 이 오브젝트와 연결)
// 마지막 인자에 0을 넘기녀 시스템의 프로세서(코어)만큼 cp 오브젝트에 연결할 스레드 수 허용
void IOCP::CreateCompletionPort(HANDLE fileHandle, HANDLE existingCompletionPort, ULONG_PTR completionKey, DWORD numberOfConcurrentThreads) {
	hComPort = CreateIoCompletionPort(fileHandle, existingCompletionPort, completionKey, numberOfConcurrentThreads);
}

// 내 CPU의 코어는 6개로 총 12개의 스레드가 생성되어 hComPort객체에 연결
void IOCP::MakeThread(){
	GetSystemInfo(&sysInfo);
	
	// c++에서 스레드 생성할 때 join을 사용해서 스레드가 종료하는 것을 확인하겠다고 명시해야함
	// 명시하지 않으면 abort() has been called 에러 발생
	for (DWORD i = 0; i < sysInfo.dwNumberOfProcessors * 2; i++)
		threadArr[i] = new std::thread(EchoThreadMain, hComPort);
}

// overlapped가 가능한 서버 소켓 생성 후 서버주소 구조체의 ipv4, 시스템 ip주소, 포트번호 초기화
void IOCP::MakeServerSocket() {
	hServSock = WSASocket(PF_INET, SOCK_STREAM, 0, NULL, 0, WSA_FLAG_OVERLAPPED);
	if (hServSock == INVALID_SOCKET)
		ErrorHandling("WSASocket() error");

	std::memset(&servAddr, 0, sizeof(servAddr));
	servAddr.sin_family = AF_INET;
	servAddr.sin_addr.s_addr = htonl(INADDR_ANY);
	servAddr.sin_port = htons(port);
}

// 서버소켓에 지정한 ip주소와 포트번호를 바인딩(운영체제에 소켓 정보를 등록해두어야 클라이언트가 송신한 패킷을 수신할 수 있음)
void IOCP::SocketBinding() const {
	if (bind(hServSock, (SOCKADDR*)&servAddr, sizeof(servAddr)) == SOCKET_ERROR)
		ErrorHandling("bind() error");

	std::cout << "binding..." << std::endl;
}

// 소켓을 수신상태로 둠으로써 클라이언트의 연결 요청을 수락(대기 큐의 크기는 5)
void IOCP::SocketListening() const {
	if (listen(hServSock, 5) == SOCKET_ERROR)
		ErrorHandling("listen() error");

	std::cout << "listening..." << std::endl;
}

// 연결 요청 받는 루틴
void IOCP::AcceptRoutine() {
	while (true) {
		SOCKET hClntSock;
		SOCKADDR_IN clntAddr;
		int clntAddrSize = sizeof(clntAddr);

		// 클라이언트의 연결요청을 수락하고 통신하기 위한 전용 소켓 생성
		hClntSock = accept(hServSock, (SOCKADDR*)&clntAddr, &clntAddrSize);
		if (hClntSock == INVALID_SOCKET)
			ErrorHandling("accept() error");

		// 클라이언트 정보 추가
		CLNT::clntArr[CLNT::numOfClnt++] = hClntSock;

		// 클라이언트 소켓(번호)과 주소를 저장할 clntInfo 구조체 초기화
		clntInfo = new CLNT_INFO;
		clntInfo->hClntSock = hClntSock;
		memcpy(&(clntInfo->clntAddr), &clntAddr, clntAddrSize);

		// completion port 커널 오브젝트에 클라이언트 소켓 연결
		CreateIoCompletionPort((HANDLE)hClntSock, hComPort, (DWORD)clntInfo, 0);

		// 데이터 송수신에 관한 정보를 저장할 ioInfo 구조체 초기화
		ioInfo = new IO_INFO;
		memset(&(ioInfo->overlapped), 0, sizeof(OVERLAPPED));
		ioInfo->wsaBuf.len = BUF_SIZE;
		ioInfo->wsaBuf.buf = ioInfo->buffer;
		ioInfo->rwMode = READ;

		// 클라이언트로부터 오는 메시지를 먼저 수신
		// 6번째 인자로 ioInfo 구조체의 overlapped 주소를 전달하게 되면 ioInfo 구조체의 다른 변수들도 접근 가능하다.
		// 왜냐하면 ioInfo 구조체의 주소가 이 구조체의 첫번째 변수인 overlapped 주소와 동일하기 때문
		WSARecv(clntInfo->hClntSock, &(ioInfo->wsaBuf), 1, &dwRecvBytes, &dwFlags, &(ioInfo->overlapped), NULL);
	}

	// 생성한 스레드가 종료될 때 까지 현재 스레드(main thread)를 블로킹시킨다.
	// 스레드가 종료되면 join()함수가 반환되고 모든 스레드가 종료되면 모든 join()함수가 반환되어 main thread가 계속 진행
	for (DWORD i = 0; i < sysInfo.dwNumberOfProcessors * 2; i++)
		threadArr[i]->join();
}

// 스레드가 실행하는 함수는 비 멤버 함수거나 정적함수여야 하기 때문에 전역함수 또는 static으로 정적함수로 만들어야 한다.
// 전역/static 함수는 객체 생성 전에 이미 정의되기 때문에 멤버 변수 접근을 할 수 없다.
// 해결책: 이름공간(namespace)을 정의해서 접근하자
void EchoThreadMain(HANDLE hComPort) {
	SOCKET sock;
	DWORD bytesTrans;
	LP_CLNT_INFO clntInfo;
	LP_IO_INFO ioInfo;
	DWORD flags = 0;
	char tempBuf[BUF_SIZE];

	while (true) {
		// IO Completion Queue에 완료통지 항목이 존재하면 GetQueuedCompletionStatus 함수가 반환한다.
		// 반환하면서 3번째 인자(clntInfo)에는 CreateIoCompletionPort함수의 3번째 인자(clntInfo)가 전달되고
		// 4번째인자(ioInfo)에는 WSARecv 또는 WSASend의 6번째 인자&(ioInfo->overlapped)가 전달된다.
		GetQueuedCompletionStatus(hComPort, &bytesTrans, (LPDWORD)&clntInfo, (LPOVERLAPPED*)&ioInfo, INFINITE);

		// clntInfo 구조체에서 클라이언트 소켓 정보 가져옴
		sock = clntInfo->hClntSock;

		// 데이터의 수신이라면
		if (ioInfo->rwMode == READ) {
			std::cout << "message received" << std::endl;

			// bytesTrans가 0이라는 것은 EOF가 전송된 것
			if (bytesTrans == 0) {
				// 소켓을 닫고 동적할당한 메모리(클라이언트 정보, IO 정보)를 해제
				closesocket(sock);
				delete clntInfo;
				delete ioInfo;

				// 소켓 연결을 종료한 클라이언트는 메시지를 보낼 필요가 없기 때문에 정보 제거
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

			// bytesTrans가 0이 아니라면 어떤 메시지가 전송된 것
			// 수신한 데이터를 echo 하기 위해 임시 버퍼에 복사 후 ioInfo 메모리 해제
			// clntInfo는 계속해서 사용해야 하기 때문에 메모리 해제 하지 않음
			strcpy(tempBuf, ioInfo->buffer);
			delete ioInfo;

			// 서버에 접속한 모든 클라이언트에게 채팅 전송
			// 매 출력마다 새로운 OVERLAPPED 구조체 동적 할당
			for (int i = 0; i < CLNT::numOfClnt; i++) {
				ioInfo = new IO_INFO;
				memset(&(ioInfo->overlapped), 0, sizeof(OVERLAPPED));
				ioInfo->wsaBuf.len = bytesTrans;
				ioInfo->wsaBuf.buf = tempBuf;
				ioInfo->rwMode = WRITE;
				WSASend(CLNT::clntArr[i], &(ioInfo->wsaBuf), 1, NULL, 0, &(ioInfo->overlapped), NULL);
			}

			// 채팅을 전송했으면 또 다른 채팅을 받을 수 있어야함
			ioInfo = new IO_INFO;
			memset(&(ioInfo->overlapped), 0, sizeof(OVERLAPPED));
			ioInfo->wsaBuf.len = BUF_SIZE;
			ioInfo->wsaBuf.buf = ioInfo->buffer;
			ioInfo->rwMode = READ;
			WSARecv(sock, &(ioInfo->wsaBuf), 1, NULL, &flags, &(ioInfo->overlapped), NULL);
		}
		else {
			// 출력이 완료되면 IO정보는 메모리 해제한다.
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

// 복사 생성자를 사용하지는 않지만 기본 정의는 해주자
IOCP::IOCP(const IOCP& ref) {
	wsaData = ref.wsaData;
	hComPort = ref.hComPort;
	sysInfo = ref.sysInfo;
	
	// 동적 할당의 경우 같은 메모리를 가리키지 않게 새로 할당해서 복사
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

	//스레드는 복사하지 않을 것임

	port = ref.port;
}

// 대입 연산자를 사용하지는 않지만 오버로딩 해두자
IOCP& IOCP::operator=(const IOCP& ref) {
	// 메모리 누수를 방지하기 위해 동적할당 받은 메모리 미리 해제
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

	//스레드는 복사하지 않을 것임

	port = ref.port;

	return *this; // 객체 자신을 의미하는 참조자를 반환하면 실행문에서 연속으로 연산이 가능
}