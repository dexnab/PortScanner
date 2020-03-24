#include <stdio.h>
#include <Windows.h>
#pragma comment(lib,"WS2_32.lib")

#define CON_TIMEOUT 2000//���ӳ�ʱʱ�䣨��λms��

//��Ϊ���͸��ȴ����ӵ��̺߳���<ConnectProc>�Ĳ���
struct ConnectParam
{
	SOCKET shSocket;//���̴߳����Ľ������ӵ��׽���
	struct sockaddr_in addrRem;//�Է���ַ
	BOOL iSucceed;//���ڷ����Ƿ����ӳɹ� FALSE: ʧ��; TRUE: �ɹ�
};

struct sockaddr_in g_addrLoc, g_addrRem;
BOOL iStart = FALSE;//FALSE: ����ʼ; TRUE: ��ʼ

//���ڵȴ����ӵ��̣߳��������ʱ�䳬��<CON_TIMEOUT>��ֵ���ᱻ���߳���ֹ
//�������<ConnectParam>
DWORD WINAPI ConnectProc(LPVOID lpParam)
{
	int err = 0;

	err = connect(((struct ConnectParam*)lpParam)->shSocket, &((struct ConnectParam*)lpParam)->addrRem, sizeof(struct sockaddr_in));

	if (err != 0) ((struct ConnectParam*)lpParam)->iSucceed = FALSE;
	else ((struct ConnectParam*)lpParam)->iSucceed = TRUE;

	return 0;
}

//ɨ�赥���ֶΣ�128���˿ڣ������߳�
//����Ϊ�߳������ţ�����ȷ���为��ɨ��Ķ˿ڷֶ�
DWORD WINAPI ScanProc(LPVOID lpParam)
{
	int nTid = *(int*)lpParam;
	int nCurPort = 0, err = 0;
	struct ConnectParam cpParam;//���͸��ȴ����ӵ��̺߳���<ConnectProc>�Ĳ���
	HANDLE hThread;//�����̵߳��߳̾��

	//��֤
	if (nTid > 511) return -1;

	//��ʼ��
	cpParam.addrRem = g_addrRem;
	nCurPort = 128 * nTid;

	//Report TID
	printf("Thread %d started!\n", nTid);

	//Wait for start signal
	while (iStart == FALSE) Sleep(1000);

	//0���̻߳��У������ķֿ���
	if (nTid == 0) printf("\n");

	//�����¿�ʼѭ����
T_Start:
	//����˿ڸ���
	cpParam.addrRem.sin_port = htons(nCurPort);

	//Init socket
	cpParam.shSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);

	//��
	err = bind(cpParam.shSocket, &g_addrLoc, sizeof(struct sockaddr_in));
	if (err != 0)//�����ʧ�ܣ��Ǿ���100ms�����°�
	{
		Sleep(100);
		goto T_Start;
	}

	//��ʼ��
	cpParam.iSucceed = FALSE;

T_Connect:
	hThread = CreateThread(NULL, NULL, ConnectProc, &cpParam, 0, NULL);
	if (hThread == NULL) goto T_Connect;//��֤�վ��

	WaitForSingleObject(hThread, CON_TIMEOUT);

	//ǿ����ֹ
	TerminateThread(hThread, 0);
	CloseHandle(hThread);

	//�ж��Ƿ�ɹ�
	if (cpParam.iSucceed == TRUE)//�ɹ�
	{
		printf("NOTICE: Port %d open!\n", nCurPort);
		closesocket(cpParam.shSocket);
	}
	else//ʧ��
	{

		closesocket(cpParam.shSocket);
	}

	//��0���̴߳����������
	if (nTid == 0) printf("Complete: (%%%d)\t\r", nCurPort * 100 / 127);


	//�Ķ˿�
	nCurPort++;

	//�ж��Ƿ�ɨ��
	if (nCurPort <= nTid * 128 + 127) goto T_Start;
	//else printf("Thread %d end!\n", nTid);

	//�ȴ������߳̽���
	Sleep(5000);

	return 0;
}

//ɨ�����˿�
void SingleConnect()
{
	int nPort, err = 0;
	SOCKET shSocket;
	struct sockaddr_in addrRem = g_addrRem;

	printf("Input port: ");
	scanf_s("%d", &nPort, sizeof(nPort));

	addrRem.sin_port = htons(nPort);

	//��ʼ���׽���
	shSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);

T_Bind:
	err = bind(shSocket, &g_addrLoc, sizeof(struct sockaddr_in));
	if (err != 0)//��ʧ�ܣ����°�
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

	//��ʼ����ֵ
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
