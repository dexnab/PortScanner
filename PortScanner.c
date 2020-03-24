#include <stdio.h>
#include <Windows.h>
#pragma comment(lib,"WS2_32.lib")

#define CON_TIMEOUT 2000//连接超时时间（单位ms）

//作为传送给等待连接的线程函数<ConnectProc>的参数
struct ConnectParam
{
	SOCKET shSocket;//父线程创建的将被连接的套接字
	struct sockaddr_in addrRem;//对方地址
	BOOL iSucceed;//用于返回是否连接成功 FALSE: 失败; TRUE: 成功
};

struct sockaddr_in g_addrLoc, g_addrRem;
BOOL iStart = FALSE;//FALSE: 不开始; TRUE: 开始

//用于等待连接的线程，如果运行时间超过<CON_TIMEOUT>的值将会被父线程终止
//参数详见<ConnectParam>
DWORD WINAPI ConnectProc(LPVOID lpParam)
{
	int err = 0;

	err = connect(((struct ConnectParam*)lpParam)->shSocket, &((struct ConnectParam*)lpParam)->addrRem, sizeof(struct sockaddr_in));

	if (err != 0) ((struct ConnectParam*)lpParam)->iSucceed = FALSE;
	else ((struct ConnectParam*)lpParam)->iSucceed = TRUE;

	return 0;
}

//扫描单独分段（128个端口）的子线程
//参数为线程索引号，用于确定其负责扫描的端口分段
DWORD WINAPI ScanProc(LPVOID lpParam)
{
	int nTid = *(int*)lpParam;
	int nCurPort = 0, err = 0;
	struct ConnectParam cpParam;//传送给等待连接的线程函数<ConnectProc>的参数
	HANDLE hThread;//连接线程的线程句柄

	//验证
	if (nTid > 511) return -1;

	//初始化
	cpParam.addrRem = g_addrRem;
	nCurPort = 128 * nTid;

	//Report TID
	printf("Thread %d started!\n", nTid);

	//Wait for start signal
	while (iStart == FALSE) Sleep(1000);

	//0号线程换行（与上文分开）
	if (nTid == 0) printf("\n");

	//（以下开始循环）
T_Start:
	//保存端口更改
	cpParam.addrRem.sin_port = htons(nCurPort);

	//Init socket
	cpParam.shSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);

	//绑定
	err = bind(cpParam.shSocket, &g_addrLoc, sizeof(struct sockaddr_in));
	if (err != 0)//如果绑定失败，那就在100ms后重新绑定
	{
		Sleep(100);
		goto T_Start;
	}

	//初始化
	cpParam.iSucceed = FALSE;

T_Connect:
	hThread = CreateThread(NULL, NULL, ConnectProc, &cpParam, 0, NULL);
	if (hThread == NULL) goto T_Connect;//验证空句柄

	WaitForSingleObject(hThread, CON_TIMEOUT);

	//强制终止
	TerminateThread(hThread, 0);
	CloseHandle(hThread);

	//判断是否成功
	if (cpParam.iSucceed == TRUE)//成功
	{
		printf("NOTICE: Port %d open!\n", nCurPort);
		closesocket(cpParam.shSocket);
	}
	else//失败
	{

		closesocket(cpParam.shSocket);
	}

	//用0号线程代表总体进度
	if (nTid == 0) printf("Complete: (%%%d)\t\r", nCurPort * 100 / 127);


	//改端口
	nCurPort++;

	//判断是否扫完
	if (nCurPort <= nTid * 128 + 127) goto T_Start;
	//else printf("Thread %d end!\n", nTid);

	//等待其他线程结束
	Sleep(5000);

	return 0;
}

//扫单个端口
void SingleConnect()
{
	int nPort, err = 0;
	SOCKET shSocket;
	struct sockaddr_in addrRem = g_addrRem;

	printf("Input port: ");
	scanf_s("%d", &nPort, sizeof(nPort));

	addrRem.sin_port = htons(nPort);

	//初始化套接字
	shSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);

T_Bind:
	err = bind(shSocket, &g_addrLoc, sizeof(struct sockaddr_in));
	if (err != 0)//绑定失败，重新绑定
	{
		Sleep(100);
		goto T_Bind;
	}

	err = connect(shSocket, &addrRem, sizeof(struct sockaddr_in));
	if (err != 0)
	{
		printf("Close!\n");
	}
	else
	{
		printf("Open!\n");
	}

	closesocket(shSocket);
	return 0;
}

int main()
{
	char strTarget[20];
	HANDLE hThread[512];
	WSADATA LibVer;
	int i = 0;

	memset(hThread, 0x00, 512);
	memset(strTarget, 0xcc, 20);

	printf("Input target IP (domain name cannot be recognized): ");
	scanf_s("%s", strTarget, sizeof(strTarget));
	printf("Input mode: 1) Single, 2) All: ");
	scanf_s("%d", &i, sizeof(i));

	//初始化赋值
	g_addrLoc.sin_family = AF_INET;
	g_addrLoc.sin_addr.S_un.S_addr = INADDR_ANY;
	g_addrLoc.sin_port = 0;
	g_addrRem.sin_family = AF_INET;
	g_addrRem.sin_addr.S_un.S_addr = inet_addr(strTarget);

	//Init WSA
	WSAStartup(MAKEWORD(2, 0), &LibVer);

	if (i == 1)
	{
		SingleConnect();
		WSACleanup();
		system("pause");
		return 0;
	}

	for (i = 0; i <= 511; i++)
	{
	T_CreateTH:
		hThread[i] = CreateThread(NULL, NULL, ScanProc, &i, 0, NULL);
		if (hThread[i] == NULL) goto T_CreateTH;

		Sleep(10);
	}

	iStart = TRUE;

	WaitForSingleObject(hThread[0], INFINITE);

	printf("Scanning finished!\n");

	for (i = 0; i <= 511; i++)
	{
		CloseHandle(hThread[i]);
	}

	WSACleanup();
	system("pause");
	return 0;
}
