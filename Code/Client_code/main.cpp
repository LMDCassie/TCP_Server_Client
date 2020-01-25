#include <stdio.h>
#include <WinSock2.h>
#include "Client.h"


BOOL CONNECTION_STATUS = FALSE;

HANDLE hEvent_User = NULL;

HANDLE hEvent_Receiving = NULL;

char recvBuf[512] = { 0 };

SOCKET Client_SOCKET = 0;


int main() {

	Initialize_WinSocket();

	CreateSubThread_User();

	GetHelp();

	while (true)
	{

		if (WaitForSingleObject(hEvent_User, 10) != WAIT_OBJECT_0)  //NO user input
		{

		}
		else 
		{
			ResetEvent(hEvent_User);
			Sleep(1);
		}

		if (WaitForSingleObject(hEvent_Receiving, 10) != WAIT_OBJECT_0) //NO receiving data
		{

		}
		else  
		{
			ResetEvent(hEvent_Receiving);
			print();
			Sleep(1);
		}
	}

	return 0;
}


void Initialize_WinSocket() {

	WSADATA wsaData;
	WORD wVersionRequested = MAKEWORD(2, 2); 

	if (WSAStartup(wVersionRequested, &wsaData) != 0)
	{
		printf("WSAStartup() failed!\n");
		return;
	}

	if (LOBYTE(wsaData.wVersion) != 2 || HIBYTE(wsaData.wVersion) != 2)
	{
		WSACleanup();
		printf("Invalid Winsock version!\n");
		return;
	}
}


SOCKET Socket_Systemcall() {

	SOCKET mSOCKET = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (mSOCKET == INVALID_SOCKET)
	{
		WSACleanup();
		printf("socket() failed!\n");
		return 0; 
	}
	printf("SOCKET: %lu\n", mSOCKET);

	//Create User_Pipeline's eventMsg ( , mannul, reset , init , )
	hEvent_User = CreateEvent(NULL, TRUE, FALSE, NULL);

	return mSOCKET;
}


DWORD WINAPI User_Pipeline(LPVOID lpParameter) {
	printf("SubThread_userPipeline ID:%4d created!\n", GetCurrentThreadId());

	while (true) {
		char _input = getchar();
		int cnt = 0;
		switch (_input) {
		case USER_OP_CONNECT:
			Connect2Server();break;
		case USER_OP_TIME:
		    getTime();
			break;
		case USER_OP_NAME:
			getName();break;
		case USER_OP_LIST:
			getClientList();break;
		case USER_OP_SEND:
			forward();break;
		case USER_OP_BREAKCONN:
			DisconnetfromServer();break;
		case USER_OP_QUIT:
			QuitProgram();break;
		case USER_OP_HELP:
			GetHelp();break;
		default:
			break;
		}
		SetEvent(hEvent_User);
	}
}


void CreateSubThread_User() {
	HANDLE hThread = NULL;
	DWORD  threadId = 0;

	hThread = CreateThread(NULL, 0, User_Pipeline, NULL, 0, &threadId);
	if (hThread == NULL || threadId == 0)
	{
		printf("CreatThread failed.\n");
	}
}


DWORD WINAPI Receive_Pipeline(LPVOID lpParameter) {
	printf("SubThread_Receiving ID:%4d created!\n", GetCurrentThreadId());
	PrintDash();
	int ret = 0;

	//GetMessage, PostThreadMessage, PostMessage

	while (true)

	{
		ret = recv(Client_SOCKET, recvBuf, sizeof(recvBuf), 0);

		if (ret <= 0)
		{
			printf("recv failed!\n");
			break;
		}

		struct MsgPkt* temp = (struct MsgPkt*)malloc(sizeof(struct MsgPkt));
		UnPackageMsg(recvBuf, temp);

		if (temp->IP[0] == 'q')
		{
			printf("Disconnected\n");
			break;
		}
		strcpy(recvBuf, temp->content);
		SetEvent(hEvent_Receiving);
	}

	printf("SubThread ID:%d stop!\n", GetCurrentThreadId());
	PrintDash();
	//don't forget to close the client socket
	closesocket(Client_SOCKET);

	return 1;
}


void CreateSubThread_Receiving() {
	HANDLE hThread = NULL;
	DWORD  threadId = 0;

	hThread = CreateThread(NULL, 0, Receive_Pipeline, NULL, 0, &threadId);
	if (hThread == NULL || threadId == 0)
	{
		printf("CreatThread failed.\n");
	}
}


void Connect2Server() {

	u_long _serverPort = 0;
	char  _serverIP[64] = { 0 };
	printf("Please type in your target server's IP:  ");
	scanf("%s", _serverIP);
	printf("Please type in your target server's Port:  ");
	scanf("%lu", &_serverPort);

	//服务器地址信息对象
	SOCKADDR_IN saServer;
	//地址家族
	saServer.sin_family = AF_INET;
	//服务器端口号
	saServer.sin_port = htons(_serverPort); //"Host to Network Long"
											//服务器IP
	saServer.sin_addr.s_addr = inet_addr(_serverIP);

	Client_SOCKET = Socket_Systemcall();

	int ret = connect(Client_SOCKET, (PSOCKADDR)&saServer, sizeof(saServer));
	if (ret == SOCKET_ERROR)
	{
		printf("connect() failed!\n");
		closesocket(Client_SOCKET);  //关闭套接字
		WSACleanup();
		return;
	}
	//recvmsg();
	//sendmsg();

	CONNECTION_STATUS = TRUE;

	//TODO
	//Create Receive_Pipeline's eventMsg ( , mannul, reset , init , )
	hEvent_Receiving = CreateEvent(NULL, TRUE, FALSE, NULL);

	//Create sub-threads
	CreateSubThread_Receiving();

}


void getTime() {

	struct RequestPkt* temp = (struct RequestPkt*)malloc(sizeof(struct RequestPkt));
	char* s = (char*)malloc(sizeof(char));
	strcpy(temp->type, "time");
	PktRequest(temp, s);
	int ret = send(Client_SOCKET, s, strlen(s), 0);
	if (ret == SOCKET_ERROR)
	{
		printf("getTime send() failed!\n");
	}
}

void getName() {

	struct RequestPkt* temp = (struct RequestPkt*)malloc(sizeof(struct RequestPkt));
	char* s = (char*)malloc(sizeof(char));
	strcpy(temp->type, "name");
	PktRequest(temp, s);
	int ret = send(Client_SOCKET, s, strlen(s), 0);
	if (ret == SOCKET_ERROR)
	{
		printf("getName send() failed!\n");
	}
}

void getClientList() {

	struct RequestPkt* temp = (struct RequestPkt*)malloc(sizeof(struct RequestPkt));
	strcpy(temp->type, "list");

	char* s = (char*)malloc(sizeof(char));
	PktRequest(temp, s);

	int ret = send(Client_SOCKET, s, strlen(s), 0);
	if (ret == SOCKET_ERROR)
	{
		printf("getClientList send() failed!\n");
	}
}

void forward()
{
	struct RequestPkt* temp = (struct RequestPkt*)malloc(sizeof(struct RequestPkt));

	strcpy(temp->type, "msg");

	printf("plz input your target index:  ");
	scanf("%s", temp->number);
	getchar(); //get rid of the '\n'
	printf("plz input your target msg:  ");
	gets_s(temp->content, MAXCONTENT);

	char* s = (char*)malloc(sizeof(char));
	PktRequest(temp, s);
	int ret = send(Client_SOCKET, s, strlen(s), 0);
	if (ret == SOCKET_ERROR)
	{
		printf("forward send() failed!\n");
	}
}


void QuitProgram() {
	if (CONNECTION_STATUS == TRUE) {
		DisconnetfromServer();
	}

	exit(0);
}


void DisconnetfromServer() {
	CONNECTION_STATUS = FALSE;

	//TODO
	//Close sub-threads
	//CloseHandle
	struct RequestPkt* temp = (struct RequestPkt*)malloc(sizeof(struct RequestPkt));
	char* s = (char*)malloc(sizeof(char));
	strcpy(temp->type, "quit");
	PktRequest(temp, s);
	int ret = send(Client_SOCKET, s, strlen(s), 0);
	if (ret == SOCKET_ERROR)
	{
		printf("DisconnetformServer send() failed!\n");
	}
}


void print() {

	printf("%s\n", recvBuf);
	printf("-----------------------------\n");
}


void PktRequest(struct RequestPkt* t, char* s) {
	strcpy(s, "\0");           //clean s
	strcpy(s, "|");            //"|" means a split character
	strcat(s, t->type);        //request type 
	if (strcmp(t->type, "msg") == 0)       //if type is message
	{
		strcat(s, "|");
		strcat(s, t->number);
		strcat(s, "|");
		strcat(s, t->content);
	}
	strcat(s, "|");            //  "|type|number|content|"
}


void PktMsg(struct MsgPkt* t, char* s)
{
	strcpy(s, "\0");           //clean s
	strcpy(s, "|");
	s[1] = t->msg;             //message type
	s[2] = '\0';
	strcat(s, "|");
	if (t->msg == '1' || t->msg == '3')       //message type 1 or 3
	{
		strcat(s, t->order);
		strcat(s, "|");
		strcat(s, t->IP);
		strcat(s, "|");
		strcat(s, t->port);
		strcat(s, "|");
		strcat(s, t->content);
		strcat(s, "|");
	}                          //"|1|order|IP|port|content|"
	else
	{
		strcat(s, "|");
		strcat(s, t->IP);     
		strcat(s, "|");
		if (strcmp(t->IP, "msg") == 0)
		{
			strcat(s, t->port);
			strcat(s, "|");
			strcat(s, "|");
		}                      //"|2||IP|port||"
		if (strcmp(t->IP, "list") == 0)
		{
			strcat(s, t->port);
			strcat(s, "|");
			strcat(s, t->content);
			strcat(s, "|");
		}                      //"|2||IP|port|content|"
		if (strcmp(t->IP, "name") == 0 || strcmp(t->IP, "time") == 0)
		{
			strcat(s, "|");
			strcat(s, t->content);
			strcat(s, "|");
		}                      //"|2||IP||content|"
	}
}


void UnPackageReq(char* s, struct RequestPkt* t)
{
	Cut(t->type, s, 1);
	if (s[1] == 'm')
	{
		Cut(t->number, s, 2);
		Cut(t->content, s, 3);
	}
}


void UnPackageMsg(char* s, struct MsgPkt* t)
{
	t->msg = s[1];
	Cut(t->order, s, 2);
	Cut(t->port, s, 4);
	Cut(t->IP, s, 3);
	Cut(t->content, s, 5);
	return;
}


void Cut(char* s, char* target, int _order)
{
	int i = 0;
	int j = 0;
	int k = 0;
	while (i != _order)
	{
		if (target[j++] == '|')
		{
			i++;
		}
	}
	while (target[j] != '|')
	{
		s[k++] = target[j++];
	}
	s[k] = '\0';
}


void GetHelp() {

	printf("HELP:\n\
		[1] Connet to Server               ==>  c\n\
		[2] Get server time                ==>  t\n\
		[3] Get server Name                ==>  n\n\
		[4] Get clients lists              ==>  l\n\
		[5] Send Messege to other clients  ==>  s\n\
		[6] Disconnet from Server          ==>  b\n\
		[7] Quit the program               ==>  q\n\
		[8] Get help                       ==>  h\n");

	PrintDash();
}

void PrintDash() {
	printf("-----------------------------\n");
}