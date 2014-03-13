/* This is main process for the client. This process handles initialization, created childs to handle Queries from other Nodes (Peers),
 * starts the daemon to monitor the files in the shared directory and provides an user inteface.
 * This version of the program has been derived from the Client Node of Project-1. Changes have been made to include threads to handle certain tasks. 
 */
//All required header files are declared here
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <signal.h>
#include <sys/time.h>
#include <ctype.h>
#include <dirent.h> 
#include <stropts.h>
#include <sys/ioctl.h>
#include <linux/netdevice.h>
#include <unistd.h>
#include <pthread.h>
#include <time.h>
//All global variables are declared here

//Debug Variables
char NodeName[] = "NodeA";

//System and Peer System IP and Port's
char MyUPort[10] = "18000", MyIP[30] = "192.168.205.130", MyQPort[10];
int qPort = 18000, uPort = 40000, wPort;
char PeerIP[20][20], PeerPort[20][20];
int NeighbourCount;
char SourceIP[30], SourcePort[10];

//Sockets
int qPortSocket, uPortSocket, peerSocket;
//Buffers for Temporary Store
char OutBuffer[200], InBuffer[1000];

char InFileName[50];
char QMessID[50];

//Holds the current list of files and the count
char fileList[100][40];
int fileListSize;

int sPort, Sip;
int server_len, RC;
struct sockaddr_in qPortAddr, uPortAddr;

//Other Global Values
int WaitDuration = 120;
char SearchTTL[4] = "004";
int SearchSlNo = 0;

//This structure is used to pass data to a thread
struct wDEx{
  char MessBuff[1000];
  int Port,MessSize;
  char FromIP[20];
  int FromPort;
};

//This structure will hold all the required session details
struct seDe{
  char SourceIP[20];
  char SourcePort[10];
  int SPort;
  char MessID[40];
  char FileName[40];
  int  CurrTTL;
  char ReqComm[7];
  char LocalBuffer[1000];
  char LocalBufferSize;
  int UsePort;
  time_t TimeStamp;
  struct seDe* Prev;
  struct seDe* Next;
};

//This is the structure of a linked list to store the queryHit Maps, this is used to send the hitQueries back to the source
struct QhMap{
  char SourceIP[20];
  char SourcePort[20];
  char FileName[40];
  struct QhMap *Next;
  struct QhMap *Prev;
};

//This is the structure of a linked list to store the queryHit Maps, this is used to send the hitQueries back to the source
//The MessID here is a combination of "IP":"PORT":"SlNo"
struct QrMap{
  char SourceIP[20];
  char SourcePort[20];
  char MessID[40];
  struct QrMap *Next;
  struct QrMap *Prev;
};

//This linked list will contain the link for all hitQueries that are being passed back to the source
struct QrMap *QueryMap;

//This linked list will contain the list of sources of a file, as received from the search
struct seDe *SearchMapStart;
struct seDe *SearchMapEnd;

//This linked list will contain the list of sources of files that have passed through this nodes, 
//a prelimnary search will be performed on this list before a query os sent on the network
struct QrMap *hQryMap;

//Function declaration section
//This function loads the config file to get the list of all the neighbours
int LoadConfig(void);
//This function initializes the node application, setup the sockets and assigns them to listening ports, send the start command to the server
int InitializeClient(void);
//This function loads the config file
int ReadConfig(void);
//Supports ReadConfig()
void substring_ip(char*, char*, char*);
//This function finds the IP of the Node 
int GetIP(void);
//Start Query Handler
void* StartQHandler(void*);
//Worker Thread
void* HandleRequest(void*);
//Used by the worker thread to deciper the message
int getStat(struct seDe*);
//Used to check if the file is available in the system
int locateFile(char *);
//Send the hit query to the source
void sendHQRY(struct seDe*);
//Send the query to the other neighbours
void propQRY(struct seDe*);
//Checks if a message for a query was already propagated
int dupProp(struct seDe*);
//process a recieved hit query message
void processHQRY(struct seDe*);
//process file request
void sendFile(struct seDe*);
//Common function to send IP messages
void sendMessage(char *, char *, char *);
//Common function to send IP messages
void sendData(char *, char *, char *, int);
//This function reads the input from the user
int GetUserInPut(void);
//Starts the file search process
int SearchFile(void);
//Sends the search queries to all the peers from the Config file, returns the no of peer nodes it was not able to connect to
int SendSearch(void);
//Waits for 2 min for reply from all the peers, returns 0 if none of the clients replied.
int WaitForReplies(void);
//These functions are used debugging only
void condioHQRY(void);
void condioSBL(void);
//Function definition section
//This function starts some sockets and sets the default values for some global variables
int InitializeClient(void)
{
//   printf("----%s Initialization\n",NodeName);
  //Set values for various function parameters
  WaitDuration = 120;
  
  QueryMap 		= (struct QrMap*)malloc(sizeof(struct QrMap));
  SearchMapStart	= (struct seDe*)malloc(sizeof(struct seDe));
  SearchMapEnd		= (struct seDe*)malloc(sizeof(struct seDe));
  hQryMap 		= (struct QrMap*)malloc(sizeof(struct QrMap));
  
  strcpy(QueryMap->MessID,"QrMap Starting Node");
  QueryMap->Prev = NULL;
  QueryMap->Next = NULL;
  
  strcpy(SearchMapStart->FileName,"seDe Starting Node");
  SearchMapStart->Prev = NULL;
  SearchMapStart->Next = SearchMapEnd;
  SearchMapStart->CurrTTL = 0;
  
  strcpy(SearchMapEnd->FileName,"seDe End Node");
  SearchMapEnd->Prev = SearchMapStart;
  SearchMapEnd->Next = NULL;
  SearchMapEnd->CurrTTL = -1;
  
  strcpy(hQryMap->MessID,"QhMap Starting Node");
  hQryMap->Prev = NULL;
  hQryMap->Next = NULL;
  
  printf("--\tStarting the Node, file sharing service...\n");
  if(ReadConfig() != 0)
  {
    printf("--ERROR - Could not read // or the config file was empty\n");
    return -1;
  }
  
  //Get IP address of this system, exit if it was not found
  if(GetIP() != 0)
  {
    printf("--ERROR - Could not access internet sockets\n");
    return -1;
  }
  
  //Listening port for the Query Handler
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
  uPortAddr.sin_family = AF_INET;
  uPortAddr.sin_addr.s_addr = htons(INADDR_ANY);
  uPortAddr.sin_port = htons(uPort);
  server_len = sizeof(uPortAddr);
  if((bind(uPortSocket, (struct sockaddr *) &uPortAddr, server_len)) < 0)
  {
    return -2;
  }
  /*
  //Get a list of all the files in the Shared Directory (Current Directory)
  if(fList() != 0)
  {
    return -5;
  }
  */
  
  //All OK
  return 0;
}
//This function loads the config file
int ReadConfig(void)
{
//   printf("-----%s Loading Config File\n",NodeName);
  //File pointer
  FILE *fp_config;
  //For reading the complete ip and port address
  char ip_from_file[30];
  //Hardcoded config filename
  char* config_filename = "node.config";
  
  NeighbourCount = 0;
  
  //Open the config file
  if((fp_config=fopen(config_filename, "rb"))==NULL)
  {
    //printf("cannot open file for reading");
    //Return error code 1 if nothing to return
    return 1;
  }
  else
  {
    while(fgets(ip_from_file,22,fp_config)!= NULL)
    {
      //Spit the complete ip:port and store it in the global neighbour list
      substring_ip(ip_from_file, PeerIP[NeighbourCount], PeerPort[NeighbourCount]);
//       printf("-----%s\t%s\t%s\n",NodeName, PeerIP[NeighbourCount], PeerPort[NeighbourCount]);
      //Increment the neighbour count
      NeighbourCount++;
     }
     //If their are no neighbours return error code 2
     if(NeighbourCount == 0)
       return 2;
   }
   //Close file to release resources
   fclose(fp_config);
//    printf("\n-----%s NeighbourCount: %d\n",NodeName, NeighbourCount);
   return 0;
}
//Function to split  ip:port to  IP and PORT
void  substring_ip(char * ipfull, char * peerip, char * peerport)
{
  int i=0;
  int flg_colonfound=0;
  int colon_position=0;
  //printf("iplength : %d \n",strlen(ipfull));
  //Go through the ip string
    for(i=0;i<strlen(ipfull);i++)
    {
      if(ipfull[i]==':')
      {
	//Set the colon found
	flg_colonfound =1;
	//Set the colon address
	colon_position = i;
	//Set the end of id address
	peerip[i]='\0';
	//printf("colon found at : %d \n",colon_position);
	continue;
      }
      if(!flg_colonfound)
      {
	//if colon not found till now insert char to peer variable
	peerip[i] = ipfull[i];
      }
      else
      {
	//if colon has been found insert char to port variable
	peerport[i-colon_position-1] = ipfull[i];
      }
      
    }
    //printf("ip converted.last postion i= %d \n",i);
    peerport[i-colon_position-1]='\0';
 }
//This function get the IP of the current system
int GetIP(void)
{
  int TempSocket, ifs, i;
  struct ifconf ifconf;
  struct ifreq ifr[50];
  
  if((TempSocket = socket(AF_INET, SOCK_STREAM, 0)) <0)
  {
    return -1;
  }

  ifconf.ifc_buf = (char *) ifr;
  ifconf.ifc_len = sizeof ifr;

  if (ioctl(TempSocket, SIOCGIFCONF, &ifconf) == -1) 
  {
    return -1;
  }

  ifs = ifconf.ifc_len / sizeof(ifr[0]);
  for (i = 0; i < ifs; i++) 
  {
    char ip[INET_ADDRSTRLEN];
    struct sockaddr_in *s_in = (struct sockaddr_in *) &ifr[i].ifr_addr;

    if (!inet_ntop(AF_INET, &s_in->sin_addr, ip, sizeof(ip))) 
    {
      return -2;
    }
    if(strcmp(ifr[i].ifr_name,"lo") != 0)
    {
      strcpy(MyIP, ip);
//       printf("--\tSystem IP acquired: %s\n",MyIP);
      close(TempSocket);
      return 0;
    }
  }
  close(TempSocket);
  return 1;
}
//This function will accept and process file Get messages from other peers
void* StartQHandler(void* SQH)
{
  int NewConnSocketNo;
  int TempInt1, SockSize, WorkerThreadCount;
  int CurrPort;
  struct wDEx *workerMessage;
  pthread_t WorkerThreadsList[100];
  struct sockaddr_in NewConnSocket;
  
  CurrPort = qPort + 1;
  
  //Listen for messages on qPort for ever
//   printf("----%s QHandler Started\n",NodeName);
  if(listen(qPortSocket, 10) < 0)
  {
//     printf("-----%s Unexpected socket errors, QHandler thread has stopped.\n", NodeName);
    //exit(0);
    //continue;
    pthread_exit(NULL);
  }
  
  SockSize = sizeof(NewConnSocket);
  WorkerThreadCount = 0;
  
  while(1)
  {
    workerMessage = (struct wDEx *)malloc(sizeof(struct wDEx));
    workerMessage->Port = CurrPort;
    //Listen for new Connections
    if((NewConnSocketNo = accept(qPortSocket, (struct sockaddr *) &NewConnSocket, &SockSize)) < 0)
    {
      //Problem with socket connections
      continue;
    }
    strcpy(workerMessage->FromIP, inet_ntoa(NewConnSocket.sin_addr));
    workerMessage->FromPort = ntohs(NewConnSocket.sin_port);
    //Read the message
    if((workerMessage->MessSize = read(NewConnSocketNo, workerMessage->MessBuff, 1000)) < 0)
    {
      //Could not read the message
      continue;
    }
    
    if(pthread_create(WorkerThreadsList + WorkerThreadCount, NULL, HandleRequest, (void *)workerMessage) != 0)
    {
      printf("--ERROR - Could not start a worker Thread\n");
    }
    
    WorkerThreadCount++;
    WorkerThreadCount = WorkerThreadCount%100;
    
    CurrPort++;
    if(CurrPort > (qPort + 5000))
      CurrPort = qPort + 1;
  }
  
  //Program control should never come here
  printf("-----%s Unexpected condition, QHandler thread has stopped.\n", NodeName);
  pthread_exit(NULL);
}
//This function is the worker thread, this is invoked to handle unexpected queries
void* HandleRequest(void* InData)
{
  struct seDe *CurrState;
  struct wDEx *InMessage;
  
  InMessage = (struct wDEx*)InData;
  
  if((InMessage->Port == 0) || (InMessage->MessSize == 0))
  {
    //Do nothing, treat as a spurious call
    pthread_exit(NULL);
  }
  
  CurrState = (struct seDe *)malloc(sizeof(struct seDe));
  time(&CurrState->TimeStamp);
  
  InMessage->MessBuff[InMessage->MessSize] = '\0';
  
  strcpy(CurrState->LocalBuffer,InMessage->MessBuff);
  CurrState->LocalBufferSize = InMessage->MessSize;
  CurrState->UsePort = InMessage->Port;
  
  if(getStat(CurrState) != 0)
  {
    //Do nothing, error in data or TTL has expired
//     printf("-----Error from get stats\n");
    free(InMessage);
    pthread_exit(NULL);
  }
  
  if(strcmp(CurrState->ReqComm,"QRY") == 0)
  {
    //This is a query
//     printf("-----QRY Section\n");
    if(strcmp(QMessID,CurrState->MessID) == 0)
    {
      //This Node was the source of this message. Take no action
//       printf("-----\tQRY sent back\n");
      free(InMessage);
      pthread_exit(NULL);
    }

    if(locateFile(CurrState->FileName) == 0)
    {
      //File was found
      sendHQRY(CurrState);
    }
    else
    {
      //Send QRY to the other neighbours
      propQRY(CurrState);
      //This CurrState should not be deleted
    }
    //task complete
  }
  else if(strcmp(CurrState->ReqComm,"HQY") == 0)
  {
    //This is a hit query
//     printf("-----HitQRY Section\n");
    processHQRY(CurrState);
  }
  else if(strcmp(CurrState->ReqComm,"GET") == 0)
  {
    //This is a get query
//     printf("-----GET Section\n");
    //strcpy(CurrState->SourceIP,InMessage->FromIP);
    //snprintf(CurrState->SourcePort,10,"%d", InMessage->FromPort);
    sendFile(CurrState);
  }
  else
  {
    //The function getStat has failed
  }
  free(InMessage);
  pthread_exit(NULL);
}
//This function splits the incoming message into diferent segments and checks if the data is consistant.
//Possible the most imp function as this enables modularity
int getStat(struct seDe *CurrState)
{
  int i, j, k;
  char TempStr[100];
//   printf("-----%s Get Stats\n",NodeName);
  
  for(i = 0; (i < CurrState->LocalBufferSize) && (CurrState->LocalBuffer[i] != '['); i++)
  {
    CurrState->ReqComm[i] = CurrState->LocalBuffer[i];
  }
  CurrState->ReqComm[i] = '\0';
  
//   printf("-----\tComm: %s\n",CurrState->ReqComm);
  
  if(strcmp(CurrState->ReqComm,"QRY") == 0)
  {
    
    //Get the TTL
    for(i = 4; (i < CurrState->LocalBufferSize) && (CurrState->LocalBuffer[i] != ']'); i++)
    {
      TempStr[i - 4] = CurrState->LocalBuffer[i];
    }
    TempStr[i - 4] = '\0';
    j = atoi(TempStr);
//     printf("-----\tTTL: %s, %d\n",TempStr, j);
    if(j < 1)
    {
      //TTL has expired
//       printf("-----\tTTL Expired\n");
      return 1;
    }
    CurrState->CurrTTL = --j;
    
    //Get the File name
    for(++i,j = 0; (i < CurrState->LocalBufferSize) && (CurrState->LocalBuffer[i] != '['); i++, j++)
    {
      CurrState->FileName[j] = CurrState->LocalBuffer[i];
    }
    CurrState->FileName[j] = '\0';
//     printf("-----\tFile Name: %s\n",CurrState->FileName);
    if(strlen(CurrState->FileName) == 0)
    {
      return 1;
    }
      
    //Get the message id
    for(++i,j = 0; (i < CurrState->LocalBufferSize) && (CurrState->LocalBuffer[i] != ']'); i++, j++)
    {
      CurrState->MessID[j] = CurrState->LocalBuffer[i];
    }
    CurrState->MessID[j] = '\0';
    
//     printf("-----\tMessage ID: %s\n",CurrState->MessID);
    
    if(strlen(CurrState->MessID) == 0)
    {
//       printf("-----\tMessage ID is Zero: %s\n",CurrState->MessID);
      return 1;
    }
    
    //Get the source IP and Port
    for(++i,j = 0; (i < CurrState->LocalBufferSize) && (CurrState->LocalBuffer[i] != ':'); i++, j++)
    {
      CurrState->SourceIP[j] = CurrState->LocalBuffer[i];
    }
    CurrState->SourceIP[j] = '\0';
//     printf("-----\tSource IP: %s\n",CurrState->SourceIP);
    if(strlen(CurrState->SourceIP) == 0)
    {
      return 1;
    }
    
    for(++i,j = 0; (i < CurrState->LocalBufferSize) && (CurrState->LocalBuffer[i] != ']'); i++, j++)
    {
      CurrState->SourcePort[j] = CurrState->LocalBuffer[i];
    }
    CurrState->SourcePort[j] = '\0';
//     printf("-----\tSource Port: %s\n",CurrState->SourcePort);
    if(strlen(CurrState->SourcePort) == 0)
    {
      return 1;
    }
    
    return 0;
  }
  else if(strcmp(CurrState->ReqComm,"HQY") == 0)
  {
    //This is a hit query
    //Get the message id
    for(i = 4,j = 0; (i < CurrState->LocalBufferSize) && (CurrState->LocalBuffer[i] != ']'); i++, j++)
    {
      CurrState->MessID[j] = CurrState->LocalBuffer[i];
    }
    CurrState->MessID[j] = '\0';
    
//     printf("-----\tMessage ID: %s\n",CurrState->MessID);
    
    if(strlen(CurrState->MessID) == 0)
    {
      return 1;
    }
    
    //Get the source IP and Port
    for(++i,j = 0; (i < CurrState->LocalBufferSize) && (CurrState->LocalBuffer[i] != ':'); i++, j++)
    {
      CurrState->SourceIP[j] = CurrState->LocalBuffer[i];
    }
    CurrState->SourceIP[j] = '\0';
//     printf("-----\tSource IP: %s\n",CurrState->SourceIP);
    if(strlen(CurrState->SourceIP) == 0)
    {
      return 1;
    }
    
    for(++i,j = 0; (i < CurrState->LocalBufferSize) && (CurrState->LocalBuffer[i] != ']'); i++, j++)
    {
      CurrState->SourcePort[j] = CurrState->LocalBuffer[i];
    }
    CurrState->SourcePort[j] = '\0';
//     printf("-----\tSource Port: %s\n",CurrState->SourcePort);
    if(strlen(CurrState->SourcePort) == 0)
    {
      return 1;
    }
    
    return 0;
    
  }
  else if(strcmp(CurrState->ReqComm,"GET") == 0)
  {
    //This is a get query
    //Get the File name
    for(i = 4,j = 0; (i < CurrState->LocalBufferSize) && (CurrState->LocalBuffer[i] != ']'); i++, j++)
    {
      CurrState->FileName[j] = CurrState->LocalBuffer[i];
    }
    CurrState->FileName[j] = '\0';
//     printf("-----\tFile Name: %s\n",CurrState->FileName);
    if(strlen(CurrState->FileName) == 0)
    {
      return 1;
    }
    
    //Get the source IP and Port
    
    for(++i,j = 0; (i < CurrState->LocalBufferSize) && (CurrState->LocalBuffer[i] != ':'); i++, j++)
    {
      CurrState->SourceIP[j] = CurrState->LocalBuffer[i];
    }
    CurrState->SourceIP[j] = '\0';
//     printf("-----\tSource IP: %s\n",CurrState->SourceIP);
    if(strlen(CurrState->SourceIP) == 0)
    {
      return 1;
    }
    
    for(++i,j = 0; (i < CurrState->LocalBufferSize) && (CurrState->LocalBuffer[i] != ']'); i++, j++)
    {
      CurrState->SourcePort[j] = CurrState->LocalBuffer[i];
    }
    CurrState->SourcePort[j] = '\0';
//     printf("-----\tSource Port: %s\n",CurrState->SourcePort);
    if(strlen(CurrState->SourcePort) == 0)
    {
      return 1;
    }
    
    
    return 0;
    
  }
  else
  {
    //In valid command
  }
  return 1;
}
//This function will check if the file is available in the current folder
int locateFile(char *inFile)
{
  int i;
  FILE *tFp;
  
  if((tFp = fopen(inFile, "r")) == NULL)
    return 1;
  fclose(tFp);
  return 0;
  
  /*
   * //Wait if the fileList is being updated
  while(fileListSize < 0);
  
  for(i = 0; i < fileListSize; i++)
  {
    if(strcmp(inFile,fileList[i]) == 0)
      return 0;
  } 
  
  return 1;
  */
}
//This function sends the hit query to the source
void sendHQRY(struct seDe *CurrState)
{
  
  char Buffer[1000];
  
  strcpy(Buffer,"HQY[");
  strcat(Buffer, CurrState->MessID);
  strcat(Buffer,"]");
  strcat(Buffer,MyIP);
  strcat(Buffer,":");
  strcat(Buffer,MyQPort);
  strcat(Buffer,"]");
//   printf("-----\tFile Found Send HitQRY\n");
  sendMessage(CurrState->SourceIP, CurrState->SourcePort, Buffer);
}
//This program is used to send the Query message to the other neighbours
void propQRY(struct seDe *CurrState)
{
  int i;
  char Buffer[1000], TempStr[10];
  snprintf(TempStr, 10, "%d", CurrState->CurrTTL);
  
  strcpy(Buffer, "QRY[");
  strcat(Buffer,TempStr);
  strcat(Buffer,"]");
  strcat(Buffer,CurrState->FileName);
  strcat(Buffer,"[");
  strcat(Buffer,CurrState->MessID);
  strcat(Buffer,"]");
  strcat(Buffer,MyIP);
  strcat(Buffer,":");
  strcat(Buffer,MyQPort);
  strcat(Buffer,"]");
//   printf("-----\tFile Not Found Send QRY\n");
  
  if(dupProp(CurrState) == 0)
  {//This file hsd already propagated through this node
//     printf("\n----Query Dropped------\n");
    return;
  }
  for (i = 0; i < NeighbourCount; i ++)
  {
    //Send to all neighbours except the source
    if((strcmp(PeerIP[i],CurrState->SourceIP) != 0) || (strcmp(PeerPort[i],CurrState->SourcePort) != 0))
    {
      sendMessage(PeerIP[i], PeerPort[i], Buffer);
    }
  }
  
  //Add this to the list of 
  //wait if the linked list is in use
  while(SearchMapStart->CurrTTL > 0);
  SearchMapStart->CurrTTL = 1;
  CurrState->Next = SearchMapStart->Next;
  SearchMapStart->Next = CurrState;
  CurrState->Prev = SearchMapStart;
  SearchMapStart->CurrTTL = 0;
  
  //condioSBL();
}
//Checks is a query was already propagated through this node
int dupProp(struct seDe* CurrState)
{
  struct seDe *CNode;
  CNode = SearchMapStart->Next;
  
  while(CNode->CurrTTL >= 0)
  {
    if(strcmp(CNode->MessID, CurrState->MessID) == 0)
    {
      return 0;
    }
    CNode = CNode->Next;
  }
  return 1;
}
//This function is used to process hit query messages
void processHQRY(struct seDe *CurrState)
{
  struct seDe *Pointer;
  struct QrMap *NewNode;
//   printf("-----\tComparing\n\t%s\n\t%s\n",QMessID,CurrState->MessID);
  if(strcmp(QMessID,CurrState->MessID) == 0)
  {
    //This query was ordered here
    if(hQryMap->SourceIP[0] == 'C')
    {
      //stop accepting messages
      return;
    }
//     printf("-----\tHQRY for this Node, add new Entry\n");
    //wait if the queue is in use
    NewNode = (struct QrMap*)malloc(sizeof(struct QrMap));
    strcpy(NewNode->SourceIP, CurrState->SourceIP);
    strcpy(NewNode->SourcePort, CurrState->SourcePort);
    while(hQryMap->SourceIP[0] == 'U');
    //Lock the resouce
    hQryMap->SourceIP[0] = 'U';
    NewNode->Next = hQryMap->Next;
    hQryMap->Next = NewNode;
    NewNode->Prev = hQryMap;
    //Release the lock
    hQryMap->SourceIP[0] = 'N';
//     printf("-----\tHQRY for this Node, new Entry added\n");
    //condioHQRY();
  }
  else
  {
    //This has to be sent back
//     printf("-----\tHQRY not for this Node, send back\n");
    Pointer = SearchMapStart->Next;
    while(Pointer->CurrTTL >= 0)
    {
      if(strcmp(CurrState->MessID,Pointer->MessID) == 0)
      {
	//match found, send back message
	sendMessage(Pointer->SourceIP, Pointer->SourcePort, CurrState->LocalBuffer);
      }
      Pointer = Pointer->Next;
    }
    //Free the allocated memory
    free(CurrState);
  }
}
//This function is responsible for the file transfer
void sendFile(struct seDe *CurrState)
{
  char MessComm[100], FileName[100], TempStr[10];
  char MessBuff[1000], MessBuff2[1000];
  char LocalIP[20], LocalPort[10], LocalMessID[30];
  struct sockaddr_in TranSoc, TranClient;
  int TranSocNo, TranClientNo;
  int TempInt1, TempInt2, TempInt3, TempInt4, FileSize, NoOfSegments, TTL, SizeOfConn;
  FILE *fp;
//   printf("-----Transfer File Section\n");
  //strcpy(FileName, MessBuff);
//   printf("-----\tFile: %s\n-\n",CurrState->FileName);
  if((fp = fopen(CurrState->FileName, "r")) == NULL)
  {
    //Send file not available
    sendMessage(CurrState->SourceIP ,CurrState->SourcePort , "ETRAN");
    return;
  }
  //printf("-----CKP 1\n");
  //Get File stats
  fseek(fp, 0L, SEEK_END);
  FileSize = ftell(fp);
  NoOfSegments = FileSize/500;
  if((FileSize % 500) > 0)
    NoOfSegments++;
  fseek(fp, 0L, SEEK_SET);
  //printf("-----NoOfSegments: %d\n",NoOfSegments);
  
  //Setup the new transfer port
  TranSocNo = socket(AF_INET, SOCK_STREAM, 0);
  TranSoc.sin_family = AF_INET;
  TranSoc.sin_addr.s_addr = htons(INADDR_ANY);
  TranSoc.sin_port = htons(CurrState->UsePort);
  SizeOfConn = sizeof(TranSoc);
  
  bind(TranSocNo, (struct sockaddr *) &TranSoc, SizeOfConn);
  SizeOfConn = sizeof(TranClient);
  
  //Send GRPY
  strcpy(MessBuff, "GRPY[");
  snprintf(TempStr, 10, "%d", NoOfSegments);
  strcat(MessBuff, TempStr);
  strcat(MessBuff, "][");
  snprintf(TempStr, 10, "%d", CurrState->UsePort);
  strcat(MessBuff, TempStr);
  strcat(MessBuff, "]");
  
  //printf("-----To Peer: %s\n",MessBuff);
  sendMessage(CurrState->SourceIP, CurrState->SourcePort, MessBuff);
  //Wait for connection
  listen(TranSocNo, 5);
  TranClientNo = accept(TranSocNo, (struct sockaddr *) &TranClient, &SizeOfConn);
//   printf("-----\tClient Connection appected, from IP: %s, Port: %d\n",inet_ntoa(TranClient.sin_addr),ntohs(TranClient.sin_port));
 
  //Start the transfer
  strcpy(MessBuff, "FTRAN[");
  TempInt3 = 6;
  TempInt1 = 0;
  TempInt2 = 0;
  while(TempInt1 < FileSize)
  {
    if(TempInt2 >= 500)
    {
      TempInt2 = read(TranClientNo, MessBuff2, 1000);
      
      MessBuff2[TempInt2] = '\0';
//       printf("From Client: %s\n", MessBuff2);
      
      write(TranClientNo, MessBuff, TempInt3);
//       printf("Client Data sent\n\n");
      //sendData(CurrState->SourceIP ,CurrState->SourcePort , MessBuff, TempInt3);
      strcpy(MessBuff, "FTRAN[");
      TempInt3 = 6;
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
      
    MessBuff2[TempInt2] = '\0';
//     printf("From Client Exit: %s\n", MessBuff2);
    
    write(TranClientNo, MessBuff, TempInt3);
  }
  TempInt2 = read(TranClientNo, MessBuff2, 1000);
  write(TranClientNo, "ETRAN", strlen("ETRAN"));
  //Transfer is complete at this point
  //sendMessage(CurrState->SourceIP ,CurrState->SourcePort , "ETRAN");
  //printf("-----CKP 3\n");
  fclose(fp);
  //printf("-----CKP 4\n");
  close(TranClientNo);
//   printf("-----\tCKP 5\n");
  return;
}
//This is the common function to send messages
void sendMessage(char *IP, char *Port, char *Message)
{
  struct sockaddr_in TranSoc;
  int BufferLen, TranSocNo, iPort;
//   printf("-----Send Message\tIP: %s\tPort: %sMessage: \n%s\n",IP, Port, Message);
  if((TranSocNo = socket(AF_INET, SOCK_STREAM, 0)) < 0)
  {
    //Problem with new sockets
    return;
  }
  
  iPort = atoi(Port);
  //Name the socket as agreed with server.
  TranSoc.sin_family = AF_INET;
  TranSoc.sin_addr.s_addr = inet_addr(IP);
  TranSoc.sin_port = htons(iPort);

  if((connect(TranSocNo, (struct sockaddr *)&TranSoc, sizeof(TranSoc))) == 1)
  {
    //Problem with connection
    return;
  }
  //Send the Query
  write(TranSocNo, Message, strlen(Message));
  
  close(TranSocNo);
}
//This function is similar to sendMessage, but takes a stream of bytes, instead of null terminated strings
void sendData(char *IP, char *Port, char *Data, int DataLen)
{
  struct sockaddr_in TranSoc;
  int BufferLen, TranSocNo, iPort;
//   printf("-----Send Data to\tIP: %s\tPort: %s\n",IP, Port);
  if((TranSocNo = socket(AF_INET, SOCK_STREAM, 0)) < 0)
  {
    //Problem with new sockets
    return;
  }
  
  iPort = atoi(Port);
  //Name the socket as agreed with server.
  TranSoc.sin_family = AF_INET;
  TranSoc.sin_addr.s_addr = inet_addr(IP);
  TranSoc.sin_port = htons(iPort);

  if((connect(TranSocNo, (struct sockaddr *)&TranSoc, sizeof(TranSoc))) == 1)
  {
    //Problem with connection
    return;
  }
  //Send the Query
  write(TranSocNo, Data, DataLen);
  
  close(TranSocNo);
}
//This function reads the file name from the user
int GetUserInPut(void)
{
  printf("--Enter file name > ");
  scanf("%s",InFileName);
  printf("\n--\tLocating the file \"%s\".\n--\tNOTE: File with the same name in the\n  \t      working directory will be over written\n", InFileName);
  return(0);
}
//This function sends a request to the Index Server to locate the file, lists the search results and reads the user input
int SearchFile(void)
{
  int MesgLen, i, j, RC;

  //printf("----Search File\n");
  
  //Send search queries to all the peers
  if((RC = SendSearch()) > 0)
  {
    //This will indicate that some of the peer nodes were not available, not action is being taken for such nodes. The main process is reported only if all the neighbours are inaccessable
    if(RC <= NeighbourCount)
    {
      //return -1;
    }
  }
  
  //Wait for reply from the peers
  if((RC = WaitForReplies()) < 0)
  {
    //This will indicate that none of the neighbours replied within the 2 min wait limit (in which case the RC is 0, or other severe errors were encountered
    return RC;
  }
  
  return 0;
}
//This function is part of the main process and will send queries to all the peers found in the Config file
int SendSearch(void)
{
  int i, RC, AddrLen;
  char LocalOutBuffer[100], SearchSlNoStr[5];
  int LocalSocket, LOBL;
  struct sockaddr_in LocalSocketAddr;
  
  //Set the flag for the handler threads to accept the messages for self queries.
  hQryMap->SourceIP[0] = 'A';
  snprintf(SearchSlNoStr, 5, "%d", SearchSlNo++);
  //Format the message
  strcpy(LocalOutBuffer,"QRY");
  strcat(LocalOutBuffer,"[");
  strcat(LocalOutBuffer,SearchTTL);
  strcat(LocalOutBuffer,"]");
  strcat(LocalOutBuffer,InFileName);
  strcat(LocalOutBuffer,"[");
  strcat(LocalOutBuffer,MyIP);
  strcat(LocalOutBuffer,":");
  strcat(LocalOutBuffer,MyQPort);
  strcat(LocalOutBuffer,":");
  strcat(LocalOutBuffer,SearchSlNoStr);
  strcat(LocalOutBuffer,"]");
  strcat(LocalOutBuffer,MyIP);
  strcat(LocalOutBuffer,":");
  strcat(LocalOutBuffer,MyQPort);
  strcat(LocalOutBuffer,"]");
  
  strcpy(QMessID,MyIP);
  strcat(QMessID,":");
  strcat(QMessID,MyQPort);
  strcat(QMessID,":");
  strcat(QMessID,SearchSlNoStr);
  //Increment the local Search Serial Number
  //condioHQRY();
  LOBL = strlen(LocalOutBuffer);
  RC = NeighbourCount;
  for(i = 0; i < NeighbourCount; i++)
  { 
    //Start Connection
    if((LocalSocket = socket(AF_INET, SOCK_STREAM, 0)) < 0)
    {
      continue;
      return -3;
    }
    //Name the socket as agreed with server.
    LocalSocketAddr.sin_family = AF_INET;
    LocalSocketAddr.sin_addr.s_addr = inet_addr(PeerIP[i]);
    LocalSocketAddr.sin_port = htons(atoi(PeerPort[i]));
    AddrLen = sizeof(LocalSocketAddr);
    if((connect(LocalSocket, (struct sockaddr *)&LocalSocketAddr, AddrLen)) == 1)
    {
      continue;
      return -4;
    }
    
    //Send the Query
    write(LocalSocket, LocalOutBuffer, LOBL);
    RC--;
    close(LocalSocket);
  }
}
//This function will wait for replies from the client, the current wait time is 2 min
//Before this function terminates all the memory allocated for storing the replies is de-allocated
int WaitForReplies(void)
{
  int TimeSec = 0, Count = 0, ReadCount = 0;
  char Mess1[] = "Searching over the Network";
  char MessD[5][6] = {".    ","..   ","...  ",".... ","....."};
  char MessC[5][4] = {"_  "," \\ "," | "," / "," _ "};
  char MessDisplay[40], InputStr[40];
  struct QrMap *CNode, *DNode;
  
  //Wait for the 
//   printf("----Waiting for 120 sec\n");
  while(TimeSec++ < 10)
  {
    printf("\r                                              ");
    printf("\r%s %s",Mess1,MessC[TimeSec%5]);
    fflush(stdout);
    sleep(1);
  }
//   condioHQRY();
  //Check the hQryMap to see if the query Handler had any replies in the meantime, identify the right replies from the file name on the hitQuery Repository
  CNode = hQryMap->Next;
  while(CNode != NULL)
  {
    ++Count;
    CNode = CNode->Next;
  }
  
  if(Count == 0)
  {
    printf("\r                                       \r--Search Timeout\n\t The file \"%s\" was not found\n",InFileName);
    return -1;
  }
  
  //Asking the threads to stop accepting any new hitQuery messages for self
  hQryMap->SourceIP[0] = 'C';
  printf("\r                                       \r--Search Complete\n\t The file was found in the following %d locations:\n",Count);
  printf("\tSl No\tSystem\n");
  Count = 0;
  CNode = hQryMap->Next;
  while(CNode != NULL)
  {
    Count ++;
    printf("\t%5d\t%s:%s\n",Count, CNode->SourceIP,CNode->SourcePort);
    CNode = CNode->Next;
  }
  while(ReadCount++ < 4)
  {
    printf("\n--\tEnter the Serial Number from the list to down load a file\n--\tEnter \"RES\" or just hit the ENTER key to cancel this search and get back to the file request menu\n--\n");
    printf("--Enter Option > ");
    scanf("%s",InputStr);
//     printf("----Value Read %s\n",InputStr);
    //If the user wants to cancel this search
    if((strlen(InputStr) == 0) || (strcmp("RES",InputStr) == 0) || (strcmp("res",InputStr) == 0))
    {
      printf("--\tThe current search has been cleared\n");
    
      CNode = hQryMap->Next;
      hQryMap->Next = NULL;
      while(CNode != NULL)
      {
	DNode = CNode->Next;
	free(CNode);
	CNode = DNode;
      }    
      return -1;
    }
    else if((atoi(InputStr) <= 0) || (atoi(InputStr) > Count))
    {
      printf("--\tYou have entered an incorrect value, re-enter\n");
    }
    else
    {
      Count = atoi(InputStr);
//       printf("----CKP 1\n");
      CNode = hQryMap->Next;
      while(CNode != NULL && Count > 1)
      {
	--Count;
	CNode = CNode->Next;
      }
      if(CNode == NULL)
      {
	printf("--\tAn unexpected error was encountered, terminating the search\n");
      
	hQryMap->SourceIP[0] = 'C';
	
	CNode = hQryMap->Next;
	hQryMap->Next = NULL;
	while(CNode != NULL)
	{
	  DNode = CNode->Next;
	  free(CNode);
	  CNode = DNode;
	}
	return -1;
      }
//       printf("----CKP 2\n");
      strcpy(SourceIP,CNode->SourceIP);
      strcpy(SourcePort,CNode->SourcePort);
      
      hQryMap->SourceIP[0] = 'C';
//       printf("----SourceIP: %s, SourcePort: %s\n",SourceIP, SourcePort);
      CNode = hQryMap->Next;
      hQryMap->Next = NULL;
      while(CNode != NULL)
      {
	DNode = CNode->Next;
	free(CNode);
	CNode = DNode;
      }    
//       printf("----CKP 3\n");
      return 0;
    }
  }
  //The program control should never come here
  return -1;
}
//This function is responsible to obtain the file from a peer
int GetFile(void)
{
  FILE *fp;
  int PeerSocket, MesgLen, i, j, NoSegs, MoreSegs = 1, HandlerPortNo, SockSize, TempSoc;
  char SegStr[20], HandlerPort[10];
  char StatusBar[50], ch;
  struct sockaddr_in PeerAddr;
  struct sockaddr_in PeerTranAddr;
//   printf("----Get File\n");
  
  sPort = atoi(SourcePort);
//   printf("-----Connecting to: %s: %d\n", SourceIP, sPort);
  
  strcpy(OutBuffer, "GET");
  strcat(OutBuffer, "[");
  strcat(OutBuffer, InFileName);
  strcat(OutBuffer, "]");
  strcat(OutBuffer, MyIP);
  strcat(OutBuffer, ":");
  strcat(OutBuffer, MyUPort);
  strcat(OutBuffer, "]");
  SockSize = sizeof(uPortAddr);
  if((fp = fopen(InFileName, "w")) == NULL)
  {
    //Close peer socket and return
    return 0;
  }
  sendMessage(SourceIP, SourcePort, OutBuffer);
//   printf("-----Connecting to: %s:%d\n",SourceIP,sPort);
  
  if(listen(uPortSocket, 10) < 0)
  {
    printf("-----%s Unexpected socket errors, QHandler thread has stopped.\n", NodeName);
    return 0;
  }
  
  //sendMessage(SourceIP, HandlerPort, "ACK");
  if((TempSoc = accept(uPortSocket, (struct sockaddr *) &uPortAddr, &SockSize)) < 0)
  {
    printf("-----%s Unexpected socket errors, QHandler thread has stopped.\n", NodeName);
    return 0;
  }
  
  //Read the first message from the peer that indicates the number of segments and the port no of the "handler"
  if((MesgLen = read(TempSoc, InBuffer, 1000)) == 0)
  {
    fclose(fp);
    return 0;
  }
  
  //Extract data from the server message
  //
  InBuffer[MesgLen] = '\0';
  //printf("-----From peer: %s\n",InBuffer);
  for(i = 5; i < MesgLen && InBuffer[i] != ']'; i++)
  {
    SegStr[i - 5] = InBuffer[i];
  }
  SegStr[i - 5] = '\0';
//   printf("-----No of File segments: %s\n",SegStr);
  for(j = 0, i = i+2; i < MesgLen && InBuffer[i] != ']'; i++, j++)
  {
    HandlerPort[j] = InBuffer[i];
  }
  HandlerPort[j] = '\0';
//   printf("-----New Peer Port STR: %s\n",HandlerPort);
  
  HandlerPortNo = atoi(HandlerPort);
  //close(PeerSocket);
//   printf("-----New Peer Port: %d\n",HandlerPortNo);
  
  //Set up connections with the new port
  if((PeerSocket = socket(AF_INET, SOCK_STREAM, 0)) < 0)
  {
    return -3;
  }
  //Name the socket as agreed with server.
  PeerTranAddr.sin_family = AF_INET;
  PeerTranAddr.sin_addr.s_addr = inet_addr(SourceIP);
  PeerTranAddr.sin_port = htons(HandlerPortNo);
  server_len = sizeof(PeerTranAddr);
  if((RC = connect(PeerSocket, (struct sockaddr *)&PeerTranAddr, server_len)) == 1)
  {
     return -4;
  }
  
  NoSegs = atoi(SegStr);
  
  j = 0;
  
  while(MoreSegs > 0)
  {
    //Send an ACK to get a segment
    write(PeerSocket, "ACK", strlen("ACK"));
    
    MesgLen = read(PeerSocket, InBuffer, 1000);
    InBuffer[MesgLen] = '\0';
//     printf("-----From peer, len: %d, FC: %c\n\n", MesgLen, InBuffer[0]);
    
    j ++;
    if(InBuffer[0] == 'E')
    {
      //File transfer complete, let the peer know that it knows
//       printf("-----File Transfer Complete\n");
      MoreSegs = 0;
    }
    else
    {
      //Copy the file to the temp file
      for(i = 6; i < MesgLen; i++)
      {
	ch = InBuffer[i];
	fputc(ch,fp);
      }
      //Update StatusBar
    }
  }
  close(PeerSocket);
  fclose(fp);
  //Close the peer connection and all files
  return 1;
}
//This is the main function of the Client
int main(int argc, char *argv[]) 
{
  int RC = 0;
  pthread_t MainThreadsList[10];
  
  //If port number was specified, read the port number
  if(argc >= 2)
  {
    strcpy(MyUPort, argv[1]);
    uPort = atoi(MyUPort);
  }
  
  //Set the Ports
  qPort = uPort + 1;
  snprintf(MyQPort, 10, "%d", qPort);
  
  if(InitializeClient() < 0)
  {
    printf("--ERROR - Could not start the peer application, please restart the application or retry after restarting the system if the problem persists\n");
    exit(0);
  }

  //Start the query handler thread
  if(pthread_create(MainThreadsList + 0, NULL, StartQHandler, (void *)RC) != 0)
  {
    printf("--ERROR - Could not start the StartQHandler Thread\n");
    exit(0);
  }

  /*
  //Start the housekeeping thread
  if(pthread_create(MainThreadsList + 1, NULL, fMonitor, (void *)RC) != 0)
  {
    printf("--ERROR - Could not start the fMonitor Thread\n");
    exit(0);
  }
  */
  //All threads are ready and running
  printf("\n---------------------Node Ready-----------------------");
  printf("\n--\tRunning on Port: %s\n", MyUPort);
  printf("\tSearch and get the files you want.\n \tThe app is ready, enjoy!\n");
  //Stay in this loop forever
  while(1)
  {
    if(GetUserInPut() == 0)
    {
      if((RC = SearchFile()) > 0)
      {
	//Some of the peers were not reachable, no action is taken
      }
      else if(RC < 0)
      {
	//Abandoning this search because of the problems or user request
	printf("--Could not locate the file \"%s\"\n, RC was %d",InFileName, RC);
      }
      else
      {
	//The program now has the input from the source to the get the file from
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
    //remain in an infinite loop, get the next user input
  }
  //The program control should never come here
  printf("\n***Program is in an inconsistant state, terminating the Node application...\n\t...if this issue persists even after restarting the app and the system, contact the IDIOTS who developed this app***\n");
  return 0;
}
//End of App Programme

//This section has debug function
void condioHQRY(void)
{
  struct QrMap *CNode;
  CNode = hQryMap->Next;
  
//   printf("\n-CONDIO-\tHQRY Start--\n");
  while(CNode != NULL)
  {
    printf("\t%s\t%s\t%s\n",CNode->SourceIP, CNode->SourcePort, CNode->MessID);
    CNode = CNode->Next;
  }
//   printf("-CONDIO-\tHQRY END--\n\n");
}
void condioSBL(void)
{
  struct seDe *SNode;
  SNode = SearchMapStart->Next;
  
//   printf("\n-CONDIO-\tQRY Start--\n");
  while(SNode->CurrTTL >= 0)
  {
    printf("\t%s\t%s\t%s\t%d\n",SNode->SourceIP, SNode->SourcePort, SNode->MessID, SNode->CurrTTL);
    SNode = SNode->Next;
  }
//   printf("-CONDIO-\tQRY END--\n\n");
}