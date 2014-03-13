/* This is main process for the client. This process handles initialization, created childs to handle Queries from other Nodes (Peers),
 * starts the daemon to monitor the files in the shared directory and provides an user inteface.
 */
//All required header files are declared here
#include <stdio.h>
#include<stdio.h>
#include<string.h>
#include<stdlib.h>
#include<unistd.h>
#include<sys/socket.h>
#include<arpa/inet.h>
#include<netinet/in.h>
#include<signal.h>
#include<sys/time.h>
#include <ctype.h>
#include <dirent.h> 
//All global variables are declared here
char InFileName[50];
char ServerIP[30];
char MyPort[10] = "18000";
char SourceIP[30], MyIP[30] = "192.168.205.130";
char SourcePort[10];
char OutBuffer[200];
char InBuffer[1000];
char fileList[100][40];
char fileListOld[100][40];
char AddList[100][40];
char RemoveList[100][40];
int OldListSize, RemoveCount, AddCount;
int fileListSize;
int qPort = 18000;
int uPort = 40000;
int qPortSocket, uPortSocket, peerSocket;
int sPort, Sip;
struct sockaddr_in qPortAddr, uPortAddr;
//Function declarations start here
//This function initializes the node application, setup the sockets and assigns them to listening ports, send the start command to the server
int InitializeClient(void);
//Start Query Handler
void StartQHandler(void);
//Read input from the user
int GetUserInPut(void);
//Send a search request to the server and gets a reply, returns no of peers. Returns < 0 if errors are seen
int SearchFile(void);
//Retrieves the file when a suitable source is found and stores the file in the shared directory. Returns 0 if successful, > 0 if errors were found
int GetFile(void);
//Files Monitor Daemon
void fMonitor(void);
//Gets a list of all files in the Shared Directory (Current Directory)
int fList(void);
//Sends the ADD and REMOVE messages to the iServer
int fLsend(char);
//File server
void HandleRequest(int, struct sockaddr_in, int);
//Files definition section
//Called to setup all the basic sockets
int InitializeClient(void)
{
  int server_len, RC;
  printf("----Initialization\n");
  //Listening port for the Node File transfer handler
  if((qPortSocket = socket(AF_INET, SOCK_STREAM, 0)) < 0)
  {
    return -1;
  }
  qPortAddr.sin_family = AF_INET;
  qPortAddr.sin_addr.s_addr = htons(INADDR_ANY);
  qPortAddr.sin_port = htons(qPort);
  server_len = sizeof(qPortAddr);
  if((bind(qPortSocket, (struct sockaddr *) &qPortAddr, server_len)) < 0)
  {
    return -2;
  }
  
  //A socket to connect to the iServer
  if((uPortSocket = socket(AF_INET, SOCK_STREAM, 0)) < 0)
  {
    return -3;
  }
  //Name the socket as agreed with server.
  uPortAddr.sin_family = AF_INET;
  uPortAddr.sin_addr.s_addr = inet_addr(ServerIP);
  uPortAddr.sin_port = htons(uPort);
  server_len = sizeof(uPortAddr);
  if((RC = connect(uPortSocket, (struct sockaddr *)&uPortAddr, server_len)) == 1)
  {
     return -4;
  }
  
  //Get a list of all the files in the Shared Directory (Current Directory)
  if(fList() != 0)
  {
    return -5;
  }
  
  //Send Start Signal
  strcpy(OutBuffer, "SRT [");
  strcat(OutBuffer, MyIP);
  strcat(OutBuffer, ":");
  strcat(OutBuffer, MyPort);
  strcat(OutBuffer, "]");
  printf("-----To Server: %s\n",OutBuffer);
  write(uPortSocket, OutBuffer, strlen(OutBuffer));
  
  //Send the files list
  fLsend('N');
  
  return 0;
}
//This function will accept and process file Get messages from other peers
void StartQHandler(void)
{
  int NewConnSocketNo;
  int TempInt1;
  int CurrPort = 18200;
  CurrPort = qPort + 200;
  struct sockaddr_in NewConnSocket;
  //Listen for messages on MPrt for ever
  printf("----QHandler Started\n");
  if(listen(qPortSocket, 10) < 0)
  {
    printf("--------Node in an Unstable Condition, close the node App by keying Ctrl C---------\n");
    exit(0);
    //continue;
  }
  while(1)
  {
    //Listen for new Connections
    TempInt1 = sizeof(NewConnSocket);
    if((NewConnSocketNo = accept(qPortSocket, (struct sockaddr *) &NewConnSocket, &TempInt1)) < 0)
    {
      continue;
    }
    if((TempInt1 = fork()) == 0)
    {
      HandleRequest(NewConnSocketNo, NewConnSocket, CurrPort++);
      exit(0);
    }
  }
}
//This function reads the file name from the user
int GetUserInPut(void)
{
  printf("--Enter file name > ");
  scanf("%s",InFileName);
  printf("\n--\tLocating the file \"%s\".\n--\tNOTE: file with the same name in the working directory will be over written.\n", InFileName);
  return(0);
}
//This function sends a request to the Index Server to locate the file
int SearchFile(void)
{
  int MesgLen, i, j;
  strcpy(OutBuffer,"SRC [");
  strcat(OutBuffer,InFileName);
  strcpy(OutBuffer,"]");
  write(uPortSocket, OutBuffer, strlen(OutBuffer));
  MesgLen = read(uPortSocket, InBuffer, 1000);
  for(i = 9, j = 0; i < MesgLen && InBuffer[i] != ':'; i++, j++)
    SourceIP[j] = InBuffer[i];
  SourceIP[j] = '\0';
  for(j = 0, ++i; i < MesgLen && InBuffer[i] != ':';i++, j++)
    SourcePort[j] = InBuffer[i];
  SourcePort[j] = '\0';
  sPort = atoi(SourcePort);
  return 1;
}
//This function is responsible to obtain the file from a peer
int GetFile(void)
{
  FILE *fp;
  int PeerSocket, MesgLen, i, j, NoSegs, MoreSegs = 1, HandlerPortNo;
  char SegStr[20], HandlerPort[10];
  char StatusBar[50], ch;
  //Connect to the peer, return 0 if the connection could not be made
  
  //Open a pointer to a new output file
  if((fp = fopen(InFileName, "w")) == NULL)
  {
    //Close peer socket and return
    return 0;
  }
  //Send the file obtain request
  strcpy(OutBuffer, "GET ");
  strcat(OutBuffer, InFileName);
  
  //Read the first message from the peer that indicates the number of segments and the port no of the "handler"
  if((MesgLen = read(PeerSocket, InBuffer, 1000)) == 0)
  {
    //Close file, peer connection and exit
    fclose(fp);
    return 0;
  }
  for(i = 6; i < MesgLen && InBuffer[i] != ']'; i++)
  {
    SegStr[i - 6] = InBuffer[i];
  }
  for(j = 0; i < MesgLen && InBuffer[i] != ':'; i++, j++)
  {
    HandlerPort[j] = InBuffer[i];
  }
  HandlerPort[j] = '\0';
  HandlerPortNo = atoi(HandlerPort);
  //Set up connections with the new port
  
  SegStr[i] = '\0';
  NoSegs = atoi(SegStr);
  j = 0;
  while(MoreSegs > 0)
  {
    //Send an ACK to get a segment
    write(PeerSocket, "ACK", strlen("ACK"));
    MesgLen = read(PeerSocket, InBuffer, 1000);
    j ++;
    if(InBuffer[0] = 'E')
    {
      //File transfer complete, let the peer know that it knows
      MoreSegs = 0;
      //write(PeerSocket, "EACK", strlen("EACK"));
    }
    else
    {
      //Copy the file to the temp file
      for(i = 7; i < MesgLen; i++)
      {
	ch = InBuffer[i];
	fputc(ch,fp);
      }
      //Update StatusBar
    }
  }
  //Close the peer connection and all files
  return 1;
}
//Daemon to monitor the files
void fMonitor(void)
{
  
  char mBuffer[1000];
  int i,j,k;
  while(1)
  {
    sleep(30);
    RemoveCount = 0;
    AddCount = 0;
    //Copy Current File List to Old List
    for(i = 0; i < fileListSize; i++)
    {
      strcpy(fileListOld[i], fileList[i]);
    }
    OldListSize = i;
    if (fList() == 0)
    {
      for(i = 0; i < OldListSize; i++)
      {
	for(j = 0, k = 0; j < fileListSize; j++)
	{
	  if(strcmp(fileListOld[i], fileList[j]) == 0)
	  {
	    k++;
	  }
	}
	if(k == 0)
	{
	  strcpy(RemoveList[RemoveCount], fileListOld[i]);
	  RemoveCount++;
	}
      }
      for(i = 0; i < fileListSize; i++)
      {
	for(j = 0, k = 0; j < OldListSize; j++)
	{
	  if(strcmp(fileListOld[j], fileList[i]) == 0)
	  {
	    k++;
	  }
	}
	if(k == 0)
	{
	  strcpy(AddList[AddCount], fileList[i]);
	  AddCount++;
	}
      }
      if(RemoveCount > 0)
      {
	fLsend('R');
	/*
	for(i = 0; i < RemoveCount; i++)
	{
	  strcpy(mBuffer, "REM [");
	  strcat(mBuffer, RemoveList[i]);
	  strcat(mBuffer, "]");
	  write(uPortSocket, mBuffer, strlen(mBuffer);
	  sleep(1);
	}*/
      }
      if(AddCount > 0)
      {
	fLsend('A');
	/*for(i = 0; i < AddCount; i++)
	{
	  strcpy(mBuffer, "ADD [");
	  strcat(mBuffer, AddList[i]);
	  strcat(mBuffer, "]");
	  write(uPortSocket, mBuffer, strlen(mBuffer);
	  sleep(1);
	}*/
      }
    }
  }
  //Program control should never come here
  printf("--ERROR - App Node Daemon has stopped, use Ctrl C to kill the application.\n");
  exit(0);
}
//Creates a list of files in the Directory
int fList(void)
{
  DIR *SDir;
  struct dirent *lst;
  int i,j;
  i = j = 0;
  printf("----File List\n");
  SDir = opendir(".");
  while ((lst = readdir(SDir)) != NULL)
  {
    if((strcmp(".",lst->d_name) != 0)&&(strcmp("..",lst->d_name) != 0)&&(strcmp("FndFl",lst->d_name) != 0))
    {
      strcpy(fileList[i++], lst->d_name);
      printf("-----File Found: %s\n",fileList[i-1]);
    }
  }
  fileListSize = i;
  printf("-----File Count: %d\n\n",fileListSize);
  closedir(SDir);
  return 0;
}
//This function sends ADD or REMOVE messages to the iServer
int fLsend(char type)
{
  int i=0;
  printf("----File List Send\n");
  if(type == 'N')
  {
    for(i = 0; i < fileListSize; i++)
    {
      strcpy(OutBuffer, "ADD [");
      strcat(OutBuffer, MyIP);
      strcat(OutBuffer, ":");
      strcat(OutBuffer, MyPort);
      strcat(OutBuffer, "][");
      strcat(OutBuffer, fileList[i]);
      strcat(OutBuffer, "]");
      printf("-----To Server: %s\n",OutBuffer);
      write(uPortSocket, OutBuffer, strlen(OutBuffer));
      sleep(1);
    }
    return 0;
  }
  else if(type == 'A')
  {
    for(i = 0; i < AddCount; i++)
    {
      strcpy(OutBuffer, "ADD [");
      strcat(OutBuffer, MyIP);
      strcat(OutBuffer, ":");
      strcat(OutBuffer, MyPort);
      strcat(OutBuffer, "][");
      strcat(OutBuffer, AddList[i]);
      strcat(OutBuffer, "]");
      printf("-----To Server: %s\n",OutBuffer);
      write(uPortSocket, OutBuffer, strlen(OutBuffer));
      sleep(1);
    }
    return 0;
  }
  else if(type == 'R')
  {
    for(i = 0; i < RemoveCount; i++)
    {
      strcpy(OutBuffer, "REM [");
      strcat(OutBuffer, MyIP);
      strcat(OutBuffer, ":");
      strcat(OutBuffer, MyPort);
      strcat(OutBuffer, "][");
      strcat(OutBuffer, RemoveList[i]);
      strcat(OutBuffer, "]");
      printf("-----To Server: %s\n",OutBuffer);
      write(uPortSocket, OutBuffer, strlen(OutBuffer));
      sleep(1);
    }
    return 0;
  }
  else
  {
    return -1;
  }
  return -1;
}
void HandleRequest(int NewConnSocketNo, struct sockaddr_in NewConnSocket, int Port)
{
  char MessComm[4], FileName[100], TempStr[10];
  char MessBuff[1000], MessBuff2[1000];
  struct sockaddr_in TranSoc, TranClient;
  int TranSocNo, TranClientNo;
  int TempInt1, TempInt2, TempInt3, FileSize, NoOfSegments;
  FILE *fp;
  printf("----Handle Request\n");
  memset(MessBuff,' ', 1000);
  MessBuff[0] = '\0';
  MessComm[3] = '\0';
  if((TempInt1 = read(NewConnSocketNo, MessBuff, 1000)) < 0)
  {
    //continue;
    return;
  }
  for(TempInt1 = 0; TempInt1 < 3; TempInt1++)
    MessComm[TempInt1] = MessBuff[TempInt1];
  if(strcmp("GET", MessComm) != 0)
  {
    exit(0);
  }
  strcpy(FileName, &MessBuff[4]);
  if((fp = fopen(FileName, "w")) == NULL)
  {
    //Send file not available
    return;
  }
  
  //Get File stats
  fseek(fp, 0L, SEEK_END);
  FileSize = ftell(fp);
  NoOfSegments = FileSize/500;
  if((FileSize % 500) > 0)
    NoOfSegments++;
  fseek(fp, 0L, SEEK_SET);
  
  //Setup the new transfer port
  TranSocNo = socket(AF_INET, SOCK_STREAM, 0);
  TranSoc.sin_family = AF_INET;
  TranSoc.sin_addr.s_addr = htons(INADDR_ANY);
  TranSoc.sin_port = htons(Port);
  TempInt1 = sizeof(TranSoc);
  bind(TranSocNo, (struct sockaddr *) &TranSoc, TempInt1);
  listen(TranSocNo, 5);
  TempInt1 = sizeof(TranClient);
  TranClientNo = accept(TranSocNo, (struct sockaddr *) &TranClient, &TempInt1);
    
  //Send GRPY
  strcpy(MessBuff, "GRPY [");
  snprintf(TempStr, 10, "%d", NoOfSegments);
  strcat(MessBuff, TempStr);
  strcat(MessBuff, "][");
  snprintf(TempStr, 10, "%d", Port);
  strcat(MessBuff, TempStr);
  strcat(MessBuff, "]");
  write(NewConnSocketNo, MessBuff, strlen(MessBuff));
  strcpy(MessBuff, "FTRAN [");
  TempInt3 = 7;
  TempInt1 = 0;
  TempInt2 = 0;
  while(TempInt1 < FileSize)
  {
    if(TempInt2 >= 499)
    {
      TempInt2 = read(TranClientNo, MessBuff2, 1000);
      write(TranClientNo, MessBuff, TempInt3);
      strcpy(MessBuff, "FTRAN [");
      TempInt3 = 7;
      TempInt2 = 0;
    }
    else
    {
      MessBuff[TempInt3] = fgetc(fp);
      TempInt1++;
      TempInt2++;
      TempInt3++;
    }
  }
  if(TempInt2 > 0)
  {
    TempInt2 = read(TranClientNo, MessBuff2, 1000);
    write(TranClientNo, MessBuff, TempInt3);
  }
  TempInt2 = read(TranClientNo, MessBuff2, 1000);
  write(TranClientNo, "ETRAN", strlen("ETRAN"));
  
  //Transfer is complete at this point
  fclose(fp);
  close(NewConnSocketNo);
  close(TranClientNo);
  return;
}
//This is the main function of the Client
int main(int argc, char *argv[])
{
  int qHandlerPID, fMonitorPID, RC;
  if(argc <= 1)
  {
    printf("--ERROR, incomplete parameter list\n");
    printf("--Correct Format\n\t./FndFl <<Server IP>> [<<Current Port>>]\n");
    return 0;
  }
  strcpy(ServerIP, argv[1]);
  if(argc >= 3)
  {
    strcpy(MyPort, argv[2]);
    qPort = atoi(MyPort);
  }
  uPort = 40000;
  if(InitializeClient() < 0)
  {
    printf("--ERROR - Could not start the peer application, please restart the application or retry after restarting the system if problem persists\n");
    exit(0);
  }
  //Start the query handler
  if((qHandlerPID = fork()) == 0)
  {
    StartQHandler();
  }
  //Start the file monitor
  if((fMonitorPID = fork()) == 0)
  {
    fMonitor();
  }
  printf("\n---------------------Node Ready------------------------\n");
  printf("\tSearch and get the files you want.\n \tThe app is read ,enjoy!\n");
  //Stay in this loop forever
  while(1)
  {
    if(GetUserInPut() == 0)
    {
      if((RC = SearchFile()) == 0)
      {
	printf("--Could not locate the file \"%s\"\n",InFileName);
      }
      else if(RC < 0)
      {
	printf("--Could not locate the file \"%s\"\n, RC was %d",InFileName, RC);
      }
      else
      {
	printf("--\tFile found, transfer started from %s\n",SourceIP);
	if((RC = GetFile()) == 0)
	{
	  printf("--ERROR - File transfer was interrupted, restart the search\n");
	}
	else if(RC < 0)
	{
	  printf("--ERROR - File transfer was interrupted, restart the search\n");
	}
	else
	{
	  printf("--\tSuccess: file \"%s\" copied to the shared directory\n--\n", InFileName);
	}
      }
    }
  }
  //The program control should never come here
  printf("Program is in an inconsistant state, terminating the Node application\n");
  exit(0);
}