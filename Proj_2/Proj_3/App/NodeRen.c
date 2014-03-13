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
#include "headers/FileStatus.h"
#include <fcntl.h>

//Test Functions
//#include "testPackage/TestSupport.h"

//All global variables are declared here

//#define DEBUG 0;

#ifdef DEBUG
# define DEBUG_PRINT(x) printf x
#else
# define DEBUG_PRINT(x) do {} while (0)
#endif
// 
//Debug Variables
char NodeName[] = "NodeA";
int pull_interval=0;
//System and Peer System IP and Port's
char MyUPort[10] = "18000", MyIP[30] = "192.168.205.130", MyQPort[10];
int qPort = 18000, uPort = 40000, wPort;
char PeerIP[20][20], PeerPort[20][20];
int NeighbourCount;
char SourceIP[30], SourcePort[10];
char OriginIP[30], OriginPort[10];
int version;

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
  
  char OriginIP[20];
  char OriginPort[10];
  int version;
  
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
  
  char OriginIP[20];
  char OriginPort[10];
  int version;
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
void processDVL(struct seDe *CurrState);
void processVLQ(struct seDe *CurrState);
void *monitorSourceFilesModification(void * relname);

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
  
  //printf("--\tStarting the Node, file sharing service...\n");
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
  
  loadSourceFiles("./source",MyIP,MyQPort);
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
    printf("cannot open file for reading");
    //Return error code 1 if nothing to return
    return 1;
  }
  else
  {
    while(fgets(ip_from_file,22,fp_config)!= NULL)
    {
      if(ip_from_file[1]=='P')
      {
	
	char pull_intervalch[10];
	
	if(ip_from_file[4]=='H')
	{ 
	  pull_interval=0;;//so push
	}
	else
	{
	  
	  int i;
	  int j;
	  for(i=6, j=0;ip_from_file[i]!=']';i++,j++)
	  {
	    pull_intervalch[j]=ip_from_file[i];
	  }
	  pull_intervalch[j]='\0';
	  pull_interval = atoi(pull_intervalch);
	  DEBUG_PRINT(("PULL mode activated, Pull interval : %d",pull_interval));
	}
	continue;
      }
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
    printf("-----%s Unexpected socket errors, QHandler thread has stopped.\n", NodeName);
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
     DEBUG_PRINT(("-----QRY Section\n"));
    if(strcmp(QMessID,CurrState->MessID) == 0)
    {
      //This Node was the source of this message. Take no action
//       printf("-----\tQRY sent back\n");
      free(InMessage);
      pthread_exit(NULL);
    }

    //printf("-----Valid Qry. Searching File. \n");
    if(locateFile(CurrState->FileName) == 0)
    {
      //File was found
           //printf("-----File Found. Sending Hqry Section\n");
      sendHQRY(CurrState);
      
      //Propagate Query to see if the file is found any where else
      propQRY(CurrState);
    }
    else
    {
      //Send QRY to the other neighbours
       //printf("-----File not Found. Forwarding Qry Section\n");
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
  else if(strcmp(CurrState->ReqComm,"DVL") == 0)
  {
    DEBUG_PRINT(("\n DVL Received \n "));
    processDVL(CurrState);
  }
  else if(strcmp(CurrState->ReqComm,"VLQ") == 0)
  {
DEBUG_PRINT(("\n VLQ Received \n "));
    processVLQ(CurrState);
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
     //printf("-----\tTTL: %s, %d\n",TempStr, j);
    if(j < 1)
    {
      //TTL has expired
       //printf("-----\tTTL Expired\n");
      return 1;
    }
    CurrState->CurrTTL = --j;
    
    //Get the File name
    for(++i,j = 0; (i < CurrState->LocalBufferSize) && (CurrState->LocalBuffer[i] != '['); i++, j++)
    {
      CurrState->FileName[j] = CurrState->LocalBuffer[i];
    }
    CurrState->FileName[j] = '\0';
     //printf("-----\tFile Name: %s\n",CurrState->FileName);
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
    
    //printf("-----\tMessage ID: %s\n",CurrState->MessID);
    
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
     //printf("-----\tSource IP: %s\n",CurrState->SourceIP);
    if(strlen(CurrState->SourceIP) == 0)
    {
      return 1;
    }
    
    for(++i,j = 0; (i < CurrState->LocalBufferSize) && (CurrState->LocalBuffer[i] != ']'); i++, j++)
    {
      CurrState->SourcePort[j] = CurrState->LocalBuffer[i];
    }
    CurrState->SourcePort[j] = '\0';
     //printf("-----\tSource Port: %s\n",CurrState->SourcePort);
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
    
    //Get the origin IP and Port
    ++i;
    for(++i,j = 0; (i < CurrState->LocalBufferSize) && (CurrState->LocalBuffer[i] != ':'); i++, j++)
    {
      CurrState->OriginIP[j] = CurrState->LocalBuffer[i];
    }
    CurrState->OriginIP[j] = '\0';
//     printf("-----\OriginIP IP: %s\n",CurrState->OriginIP);
    if(strlen(CurrState->OriginIP) == 0)
    {
      return 1;
    }
    
    for(++i,j = 0; (i < CurrState->LocalBufferSize) && (CurrState->LocalBuffer[i] != ']'); i++, j++)
    {
      CurrState->OriginPort[j] = CurrState->LocalBuffer[i];
    }
    CurrState->OriginPort[j] = '\0';
//     printf("-----\tSource Port: %s\n",CurrState->OriginPort);
    if(strlen(CurrState->OriginPort) == 0)
    {
      return 1;
    }
    
    char version_str[10];
    for(++i,j = 0; (i < CurrState->LocalBufferSize) && (CurrState->LocalBuffer[i] != ']'); i++, j++)
    {
      version_str[j] = CurrState->LocalBuffer[i];
    }
    version_str[j]  = '\0';
    CurrState->version = atoi(version_str);
//     printf("-----\tSource Port: %s\n",CurrState->OriginPort);
    
    return 0;
    
  }
  else if(strcmp(CurrState->ReqComm,"DVL") == 0)
  {
    //This is a hit query
    //Get the message id
    for(i = 4,j = 0; (i < CurrState->LocalBufferSize) && (CurrState->LocalBuffer[i] != ']'); i++, j++)
    {
      CurrState->MessID[j] = CurrState->LocalBuffer[i];
    }
    CurrState->MessID[j] = '\0';
    
     DEBUG_PRINT(("-----\tDVL Message ID: %s LocalBufferSize : %d\n",CurrState->MessID,CurrState-> LocalBufferSize));
    
    if(strlen(CurrState->MessID) == 0)
    {
      return 1;
    }
    
    //Get the Origin IP and Port
    ++i;
    for(++i,j = 0; (i < CurrState->LocalBufferSize) && (CurrState->LocalBuffer[i] != ':'); i++, j++)
    {
      CurrState->OriginIP[j] = CurrState->LocalBuffer[i];
    }
    CurrState->OriginIP[j] = '\0';
     DEBUG_PRINT(("-----\tDVL Origin IP: %s\n",CurrState->OriginIP));
    if(strlen(CurrState->OriginIP) == 0)
    {
      return 1;
    }
    
    for(++i,j = 0; (i < CurrState->LocalBufferSize) && (CurrState->LocalBuffer[i] != ']'); i++, j++)
    {
      CurrState->OriginPort[j] = CurrState->LocalBuffer[i];
    }
    CurrState->OriginPort[j] = '\0';
     DEBUG_PRINT(("-----\tDVL Origin Port: %s\n",CurrState->OriginPort));
    if(strlen(CurrState->OriginPort) == 0)
    {
      return 1;
    }
    
    //Get the Filename
    ++i;
    for(++i,j = 0; (i < CurrState->LocalBufferSize) && (CurrState->LocalBuffer[i] != ']'); i++, j++)
    {
      CurrState->FileName[j] = CurrState->LocalBuffer[i];
    }
    CurrState->FileName[j] = '\0';
     
    DEBUG_PRINT(("-----\tDVL Filename: %s\n",CurrState->FileName));
    if(strlen(CurrState->FileName) == 0)
    {
      return 1;
    }
       
    char version_str[10];
    ++i;
    for(++i,j = 0; (i < CurrState->LocalBufferSize) && (CurrState->LocalBuffer[i] != ']'); i++, j++)
    {
      version_str[j] = CurrState->LocalBuffer[i];
    }
    version_str[j]  = '\0';
    CurrState->version = atoi(version_str);
     DEBUG_PRINT(("-----\tDVL Version: %d %s\n",CurrState->version,version_str));
    
    return 0;
    
  }
  else if(strcmp(CurrState->ReqComm,"VLQ") == 0)
  {

    for(i = 4,j = 0; (i < CurrState->LocalBufferSize) && (CurrState->LocalBuffer[i] != ']'); i++, j++)
    {
      CurrState->MessID[j] = CurrState->LocalBuffer[i];
    }
    CurrState->MessID[j] = '\0';
    
     DEBUG_PRINT(("-----\tVLQ Message ID: %s\n",CurrState->MessID));
    
    if(strlen(CurrState->MessID) == 0)
    {
      return 1;
    }
    
    //Get the Source IP and Port
    ++i;
    for(++i,j = 0; (i < CurrState->LocalBufferSize) && (CurrState->LocalBuffer[i] != ':'); i++, j++)
    {
      CurrState->SourceIP[j] = CurrState->LocalBuffer[i];
    }
    CurrState->SourceIP[j] = '\0';
     DEBUG_PRINT(("-----\tDVL Source IP: %s\n",CurrState->SourceIP));
    if(strlen(CurrState->SourceIP) == 0)
    {
      return 1;
    }
    
    for(++i,j = 0; (i < CurrState->LocalBufferSize) && (CurrState->LocalBuffer[i] != ']'); i++, j++)
    {
      CurrState->SourcePort[j] = CurrState->LocalBuffer[i];
    }
    CurrState->SourcePort[j] = '\0';
     DEBUG_PRINT(("-----\tDVL Source Port: %s\n",CurrState->SourcePort));
    if(strlen(CurrState->SourcePort) == 0)
    {
      return 1;
    }
    
    //Get the Filename
    ++i;
    for(++i,j = 0; (i < CurrState->LocalBufferSize) && (CurrState->LocalBuffer[i] != ']'); i++, j++)
    {
      CurrState->FileName[j] = CurrState->LocalBuffer[i];
    }
    CurrState->FileName[j] = '\0';
     
    DEBUG_PRINT(("-----\tDVL Filename: %s\n",CurrState->FileName));
    if(strlen(CurrState->FileName) == 0)
    {
      return 1;
    }
       
    char version_str[10];
    ++i;
    for(++i,j = 0; (i < CurrState->LocalBufferSize) && (CurrState->LocalBuffer[i] != ']'); i++, j++)
    {
      version_str[j] = CurrState->LocalBuffer[i];
    }
    version_str[j]  = '\0';
    CurrState->version = atoi(version_str);
     DEBUG_PRINT(("-----\tDVL Version: %d\n",CurrState->version));
    
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
  
  //Get valid file from the file status table
  struct fileStatus * validFile = getFileStatus(inFile);
  //printf("file status query\n");
  if(validFile == NULL)
     return 1;
  
  if(validFile->status== 'O' || validFile->status == 'V')
  {
    char filepath[70];
    if(validFile->status== 'O')
    strcpy(filepath,"./source/");
    else
    strcpy(filepath,"./downloaded/");
    
    strcat(filepath,inFile);
    if((tFp = fopen(filepath, "r")) == NULL)
    {
      fclose(tFp);
      return 1;
    }
  }
  else
    return 1;
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
  
  //Get the originip and version details form file status table
  struct fileStatus * filedet = getFileStatus(CurrState -> FileName); 
  
  strcat(Buffer,"[");
  strcat(Buffer,filedet->OriginIP);
  strcat(Buffer,":");
  strcat(Buffer,filedet->OriginPort);
  strcat(Buffer,"]");
  char ver[10] ;
  snprintf(ver,10,"%d",filedet->version);
  
  strcat(Buffer,ver);
  strcat(Buffer,"]");

  
  
//   printf("-----\tFile Found Send HitQRY\n");
  sendMessage(CurrState->SourceIP, CurrState->SourcePort, Buffer);
    //printf("Hit Query sent back\n");
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
  
  DEBUG_PRINT(("Query propagated to %d neighbours\n",NeighbourCount));
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
      DEBUG_PRINT(("Hit query received in responce\n"));
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
    
    strcpy(NewNode->OriginIP, CurrState->OriginIP);
    strcpy(NewNode->OriginPort, CurrState->OriginPort);
    NewNode->version = CurrState->version;
    
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

//Used to mark downloaded items as invalid
void processDVL(struct seDe *CurrState)
{
  struct seDe *Pointer;

   DEBUG_PRINT(("-----\tDVL Processing\n"));
  Pointer = SearchMapStart->Next;
  while(Pointer->CurrTTL >= 0)
  {
    if(strcmp(CurrState->MessID,Pointer->MessID) == 0)
    {
       DEBUG_PRINT(("-----\tDVL has already been processed once at this node\n"));
      //match found, DVL has already been done
      free(CurrState);
      return;
    }
    Pointer = Pointer->Next;
  }
  
  //Get the file entry from the file status table if it exists
  struct fileStatus * fs = getFileStatus(CurrState->FileName);
  //Check the origin 
  if(fs != NULL)
  {
    DEBUG_PRINT(("-----\tDVL File found\n"));
    if(strcmp(fs->OriginIP,CurrState->OriginIP)==0 && strcmp(fs->OriginPort,CurrState->OriginPort)==0 && fs->status != 'O')
    {
      DEBUG_PRINT(("-----\tDVL updated in file status table\n"));
      //Devalidate if from same origin by setting the status to I
      dvldtFile(fs);
      //

    }
  }
  DEBUG_PRINT(("-----\tDVL File not found\n"));
  //      chck_fileStatus();
  //    chck_fileTimeStamp();
    //Add this to the list of 
  //wait if the linked list is in use
  while(SearchMapStart->CurrTTL > 0);
  SearchMapStart->CurrTTL = 1;
  CurrState->Next = SearchMapStart->Next;
  SearchMapStart->Next = CurrState;
  CurrState->Prev = SearchMapStart;
  SearchMapStart->CurrTTL = 0;
  
  //DEBUG_PRINT(("Pull interval %d : ",pull_interval);
  //Propagate DVL if in push mode
  if(pull_interval != 0)
    return;
    
   char LocalOutBuffer[100];
  int  LOBL;
  
  DEBUG_PRINT(("-----\tPropagating DVL to neighbours\n"));
  //Format the message
  strcpy(LocalOutBuffer,"DVL");

  strcat(LocalOutBuffer,"[");
  strcat(LocalOutBuffer,CurrState->MessID);
  strcat(LocalOutBuffer,"]");
  
  //origin
  strcat(LocalOutBuffer,"[");
  strcat(LocalOutBuffer,CurrState->OriginIP);
  strcat(LocalOutBuffer,":");
  strcat(LocalOutBuffer,CurrState->OriginPort);
  strcat(LocalOutBuffer,"]");
  
  //filename
  strcat(LocalOutBuffer,"[");
  strcat(LocalOutBuffer,CurrState->FileName);
  strcat(LocalOutBuffer,"]");

  //version
  char version_str[10];
  snprintf(version_str,10,"%d",CurrState->version);
  strcat(LocalOutBuffer,"[");
  strcat(LocalOutBuffer,version_str);
  strcat(LocalOutBuffer,"]");
	

  //Increment the local Search Serial Number
  //condioHQRY();
  LOBL = strlen(LocalOutBuffer);
  int i;
  for (i = 0; i < NeighbourCount; i ++)
  {
    //Send to all neighbours except the source
    if((strcmp(PeerIP[i],CurrState->OriginIP) != 0) || (strcmp(PeerPort[i],CurrState->OriginPort) != 0))
    {
      sendMessage(PeerIP[i], PeerPort[i], LocalOutBuffer);
    }
  }
  
  return;

}

void processVLQ(struct seDe *CurrState)
{
  DEBUG_PRINT(("-----\tVLQ :Looking for file\n"));
  //Get the file entry from the file status table if it exists
  struct fileStatus * fs = getFileStatus(CurrState->FileName);
  //Check the origin 
  if(fs != NULL)
  {
       DEBUG_PRINT(("-----\tVLQ File found\n"));
    if(fs->version != CurrState->version)
    {
       DEBUG_PRINT(("-----\tSending DVL Reply\n"));
      //Send devalidate message
      //Propagate DVL
      char LocalOutBuffer[100];
      int  LOBL;
      
      //Format the message
      strcpy(LocalOutBuffer,"DVL");

      strcat(LocalOutBuffer,"[");
      strcat(LocalOutBuffer,CurrState->MessID);
      strcat(LocalOutBuffer,"]");
      
      //origin
      strcat(LocalOutBuffer,"[");
      strcat(LocalOutBuffer,fs->OriginIP);
      strcat(LocalOutBuffer,":");
      strcat(LocalOutBuffer,fs->OriginPort);
      strcat(LocalOutBuffer,"]");
      
      //filename
      strcat(LocalOutBuffer,"[");
      strcat(LocalOutBuffer,CurrState->FileName);
      strcat(LocalOutBuffer,"]");

      //version
      char version_str[10];
      snprintf(version_str,10,"%d",fs->version);
      strcat(LocalOutBuffer,"[");
      strcat(LocalOutBuffer,version_str);
      strcat(LocalOutBuffer,"]");
      
      sendMessage(CurrState->SourceIP, CurrState->SourcePort, LocalOutBuffer);
      DEBUG_PRINT(("-----\tDVL Reply Sent to %s %s with version %s\n",CurrState->SourceIP, CurrState->SourcePort,version_str));
    }

  return;
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
//printf("-----\tFile: %s\n-\n",CurrState->FileName);
  
  //Get the file from status table
  struct fileStatus * validFile = getFileStatus(CurrState->FileName);
  
  char filepath[70];
  if(validFile->status== 'O')
  strcpy(filepath,"./source/");
  else
  strcpy(filepath,"./downloaded/");
  
  strcat(filepath,CurrState->FileName);
    
  if((fp = fopen(filepath, "r")) == NULL)
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
//printf("-----\tClient Connection appected, from IP: %s, Port: %d\n",inet_ntoa(TranClient.sin_addr),ntohs(TranClient.sin_port));
 
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
//printf("From Client: %s\n", MessBuff2);
      
      write(TranClientNo, MessBuff, TempInt3);
//printf("Client Data sent\n\n");
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
//printf("From Client Exit: %s\n", MessBuff2);
    
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
//printf("-----\tCKP 5\n");
  return;
}
//This is the common function to send messages
void sendMessage(char *IP, char *Port, char *Message)
{
  struct sockaddr_in TranSoc;
  int BufferLen, TranSocNo, iPort;
  long connArgs;
  //printf("-----Send Message\tIP: %s\tPort: %sMessage: \n%s\n",IP, Port, Message);
  if((TranSocNo = socket(AF_INET, SOCK_STREAM, 0)) < 0)
  {
    //Problem with new sockets
    return;
  }
  //printf("New Socket Created\n");
  
   //Make the Connect Non Blocking to handle Connection Failures
  connArgs = fcntl(TranSocNo, F_GETFL, NULL); 
  connArgs |= O_NONBLOCK; 
  fcntl(TranSocNo, F_SETFL, connArgs); 
  
  iPort = atoi(Port);
  //Name the socket as agreed with server.
  TranSoc.sin_family = AF_INET;
  TranSoc.sin_addr.s_addr = inet_addr(IP);
  TranSoc.sin_port = htons(iPort);
  
  if((connect(TranSocNo, (struct sockaddr *)&TranSoc, sizeof(TranSoc))) == 1)
  {
    //Problem with connection
    printf("\tCould not Connect\n");
    close(TranSocNo);
    return;
  }
  //printf("\nConnected\n");
  //On Successful Connection, set the transfer to blocking again
  connArgs = fcntl(TranSocNo, F_GETFL, NULL); 
  connArgs &= (~O_NONBLOCK); 
  fcntl(TranSocNo, F_SETFL, connArgs);

  //Send the Query
  write(TranSocNo, Message, strlen(Message));
  
  close(TranSocNo);
}
//This function is similar to sendMessage, but takes a stream of bytes, instead of null terminated strings
void sendData(char *IP, char *Port, char *Data, int DataLen)
{
  struct sockaddr_in TranSoc;
  int BufferLen, TranSocNo, iPort;
  long connArgs;
//   printf("-----Send Data to\tIP: %s\tPort: %s\n",IP, Port);
  if((TranSocNo = socket(AF_INET, SOCK_STREAM, 0)) < 0)
  {
    //Problem with new sockets
    return;
  }
  
  //
  connArgs = fcntl(TranSocNo, F_GETFL, NULL); 
  connArgs |= O_NONBLOCK; 
  fcntl(TranSocNo, F_SETFL, connArgs); 
  
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
  
  //
  connArgs = fcntl(TranSocNo, F_GETFL, NULL); 
  connArgs &= (~O_NONBLOCK); 
  fcntl(TranSocNo, F_SETFL, connArgs);
  
  //Send the Query
  write(TranSocNo, Data, DataLen);
  
  close(TranSocNo);
}
//This function reads the file name from the user
int GetUserInPut(void)
{
  do
  {
    printf("--Enter file name or type show to see file satus and time stamp table > ");
    scanf("%s",InFileName);
    if(strcmp(InFileName,"show")==0)
    {
      chck_fileStatus();
      chck_fileTimeStamp();
    }
  }while(strcmp(InFileName,"show")==0);
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
   
   //printf("----Wait for replies\n");
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
  long connArgs;
  
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
  //showPeers();
  //printf("----Message Composed\n");
  //Increment the local Search Serial Number
  //condioHQRY();
  LOBL = strlen(LocalOutBuffer);
  RC = NeighbourCount;
  //printf("\n--Sending Search to %d Neighbours\n",NeighbourCount);
  for(i = 0; i < NeighbourCount; i++)
  { 
    //Start Connection
    if((LocalSocket = socket(AF_INET, SOCK_STREAM, 0)) < 0)
    {
      printf("Error in creating socket\n");
      continue;
      return -3;
    }
    
    /*
    connArgs = fcntl(LocalSocket, F_GETFL, NULL); 
    connArgs |= O_NONBLOCK; 
    fcntl(LocalSocket, F_SETFL, connArgs); 
    */
    //Name the socket as agreed with server.
    LocalSocketAddr.sin_family = AF_INET;
    LocalSocketAddr.sin_addr.s_addr = inet_addr(PeerIP[i]);
    LocalSocketAddr.sin_port = htons(atoi(PeerPort[i]));
    AddrLen = sizeof(LocalSocketAddr);
    if((connect(LocalSocket, (struct sockaddr *)&LocalSocketAddr, AddrLen)) < 0)
    {
      //printf("Error in conncting to socket\n");
      continue;
      return -4;
    }
    
    /*
    connArgs = fcntl(LocalSocket, F_GETFL, NULL); 
    connArgs &= (~O_NONBLOCK); 
    fcntl(LocalSocket, F_SETFL, connArgs);
    */
    //printf("Message being sent to neighbour %d\n",i);
    //Send the Query
    if(write(LocalSocket, LocalOutBuffer, LOBL)<0)
    {
       printf("Error writing to socket %d\n",i);
    }
    RC--;
        //printf("Closing socket\n");
    //printf("\tSearch Sent to - %s:%s\n",PeerIP[i], PeerPort[i]);
    close(LocalSocket);
  }
  //printf("----Search sent to neighbours\n");
}

//This function is part of the main process and will send queries to all the peers found in the Config file
int SendDVL(struct fileStatus * dvlFile)
{
  int i, RC, AddrLen;
  char LocalOutBuffer[100], SearchSlNoStr[5];
  int LocalSocket, LOBL;
  long connArgs;
  
  struct sockaddr_in LocalSocketAddr;
  
  struct timeval timeout;      
    timeout.tv_sec = 10;
    timeout.tv_usec = 0;
  
  //Set the flag for the handler threads to accept the messages for self queries.
  hQryMap->SourceIP[0] = 'A';
  snprintf(SearchSlNoStr, 5, "%d", SearchSlNo++);
  //Format the message
  strcpy(LocalOutBuffer,"DVL");

  strcat(LocalOutBuffer,"[");
  strcat(LocalOutBuffer,MyIP);
  strcat(LocalOutBuffer,":");
  strcat(LocalOutBuffer,MyQPort);
  strcat(LocalOutBuffer,":");
  strcat(LocalOutBuffer,SearchSlNoStr);
  strcat(LocalOutBuffer,"]");
  
  //origin
  strcat(LocalOutBuffer,"[");
  strcat(LocalOutBuffer,MyIP);
  strcat(LocalOutBuffer,":");
  strcat(LocalOutBuffer,MyQPort);
  strcat(LocalOutBuffer,"]");
  
  //filename
  strcat(LocalOutBuffer,"[");
  strcat(LocalOutBuffer,dvlFile->fname);
  strcat(LocalOutBuffer,"]");
  
  
  //version
  char version_str[10];
  snprintf(version_str,10,"%d",dvlFile->version);
  DEBUG_PRINT(("Send DVL version : %d %s \n",dvlFile->version,version_str));
  
  strcat(LocalOutBuffer,"[");
  strcat(LocalOutBuffer,version_str);
  strcat(LocalOutBuffer,"]");
	
  //printf("Sending...\n");
   DEBUG_PRINT(("Sending...\n"));
  //Increment the local Search Serial Number
  //condioHQRY();
  LOBL = strlen(LocalOutBuffer);
  RC = NeighbourCount;
  for(i = 0; i < NeighbourCount; i++)
  { 
    //Start Connection
    DEBUG_PRINT(("Creating Socket\n"));
    if((LocalSocket = socket(AF_INET, SOCK_STREAM, 0)) < 0)
    {
      DEBUG_PRINT(("\tError Socket\n"));
      continue;
      return -3;
    }
    
    //
    connArgs = fcntl(LocalSocket, F_GETFL, NULL); 
    connArgs |= O_NONBLOCK; 
    fcntl(LocalSocket, F_SETFL, connArgs); 
	
    //Name the socket as agreed with server.
    LocalSocketAddr.sin_family = AF_INET;
    LocalSocketAddr.sin_addr.s_addr = inet_addr(PeerIP[i]);
    LocalSocketAddr.sin_port = htons(atoi(PeerPort[i]));
    
    AddrLen = sizeof(LocalSocketAddr);
    DEBUG_PRINT(("Attempting Connect\n"));
    if((connect(LocalSocket, (struct sockaddr *)&LocalSocketAddr, AddrLen)) == 1)
    {
      DEBUG_PRINT(("\tError Connect\n"));
      continue;
      return -4;
    }
    DEBUG_PRINT(("\tConnected\n"));
    //
    connArgs = fcntl(LocalSocket, F_GETFL, NULL); 
    connArgs &= (~O_NONBLOCK); 
    fcntl(LocalSocket, F_SETFL, connArgs);
	
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
  
  int TimeSec = 0, Count = 0, ReadCount = 0, MasterVersion = 0, InvalidVersions = 0, TestOption = 0;
  float InvalidPercent = 0;
  char Mess1[] = "Searching over the Network";
  char MessD[5][6] = {".    ","..   ","...  ",".... ","....."};
  char MessC[5][4] = {"_  "," \\ "," | "," / "," _ "};
  char MessDisplay[40], InputStr[40];
  
  char OutFileMsg[50];
  
  struct QrMap *CNode, *DNode;
  
  //Wait for the 
  //printf("----Waiting for 120 sec\n");
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
    if(CNode->version > MasterVersion)
    {
      MasterVersion = CNode->version;
      TestOption = Count;
    }
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
    printf("\t%5d\t%s:%s\tVersion: %5d\n",Count, CNode->SourceIP,CNode->SourcePort,CNode->version);
    if(CNode->version < MasterVersion)
      InvalidVersions++;
    CNode = CNode->Next;
  }
  
  //Network Stats - This is for testing only
  InvalidPercent = (InvalidVersions*100)/Count;
  //Printng MasterVersion,No_Of_Errors,Total_Count,Percentage
  snprintf(OutFileMsg, 50, "%d,%d,%d,%f\n", MasterVersion,InvalidVersions,Count,InvalidPercent);
  //writeToFile(OutFileMsg);
  
  while(ReadCount++ < 4)
  {
    printf("\n--\tEnter the Serial Number from the list to down load a file\n--\tEnter \"RES\" to cancel this search and get back to the file request menu.\n--\n");
    printf("--Enter Option > ");
    scanf("%s",InputStr); 			// Commented for Test only
    //snprintf(InputStr,40,"%d",TestOption); 	// For Test only, MaxSerial No is chosen for test
    //printf("\n--\tOption Entered \"%s\"\n",InputStr);
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
      return -7;
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
      
      strcpy(OriginIP,CNode->OriginIP);
      strcpy(OriginPort,CNode->OriginPort);
      version = CNode->version;
      
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
//printf("----Get File\n");
  
  sPort = atoi(SourcePort);
//printf("-----Connecting to: %s: %d\n", SourceIP, sPort);
  
  strcpy(OutBuffer, "GET");
  strcat(OutBuffer, "[");
  strcat(OutBuffer, InFileName);
  strcat(OutBuffer, "]");
  strcat(OutBuffer, MyIP);
  strcat(OutBuffer, ":");
  strcat(OutBuffer, MyUPort);
  strcat(OutBuffer, "]");
  SockSize = sizeof(uPortAddr);
  
  char fpath[70];
  strcpy(fpath,"./downloaded/");
  strcat(fpath,InFileName);
  if((fp = fopen(fpath, "w")) == NULL)
  {
    //Close peer socket and return
    printf("new file cannot be created\n");
    return 0;
  }
  sendMessage(SourceIP, SourcePort, OutBuffer);
//printf("-----Connecting to: %s:%d\n",SourceIP,sPort);
  
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
//printf("-----No of File segments: %s\n",SegStr);
  for(j = 0, i = i+2; i < MesgLen && InBuffer[i] != ']'; i++, j++)
  {
    HandlerPort[j] = InBuffer[i];
  }
  HandlerPort[j] = '\0';
//printf("-----New Peer Port STR: %s\n",HandlerPort);
  
  HandlerPortNo = atoi(HandlerPort);
  //close(PeerSocket);
//printf("-----New Peer Port: %d\n",HandlerPortNo);
  
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
//printf("-----From peer, len: %d, FC: %c\n\n", MesgLen, InBuffer[0]);
    
    j ++;
    if(InBuffer[0] == 'E')
    {
      //File transfer complete, let the peer know that it knows
//printf("-----File Transfer Complete\n");
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
  
  //printf("-----Creating file status entry\n");
  //Since file is completely downloaded we create an entry in the file status linked list
  struct fileStatus * fs = createFileStatusEntry(InFileName,OriginIP,OriginPort,'V',version);
  insertNewFileStatus(fs);
  
  //Close the peer connection and all files
  return 1;
}

void SendVLQ()
{
  //Get the file origin and version from the file status table
  struct fileStatus * fs = head2;
  while(fs->next !=NULL)
  {
    fs = fs->next;
    
    //Stop if sending to same machine
    if(strcmp(fs->OriginIP,MyIP)==0 && strcmp(fs->OriginPort,MyQPort)==0)
    {
      DEBUG_PRINT(("No VLQ to self\n"));
      return;
    }
    
    char LocalOutBuffer[100], SearchSlNoStr[5];

    
    //Compose the VLQ message
    snprintf(SearchSlNoStr, 5, "%d", SearchSlNo++);
    //Format the message
    strcpy(LocalOutBuffer,"VLQ");

    strcat(LocalOutBuffer,"[");
    strcat(LocalOutBuffer,MyIP);
    strcat(LocalOutBuffer,":");
    strcat(LocalOutBuffer,MyQPort);
    strcat(LocalOutBuffer,":");
    strcat(LocalOutBuffer,SearchSlNoStr);
    strcat(LocalOutBuffer,"]");
    
      //SourceIP
    
    
    strcat(LocalOutBuffer,"[");
    strcat(LocalOutBuffer,MyIP);
    strcat(LocalOutBuffer,":");
    strcat(LocalOutBuffer,MyQPort);
    strcat(LocalOutBuffer,"]");
    
    //filename
    strcat(LocalOutBuffer,"[");
    strcat(LocalOutBuffer,fs->fname);
    strcat(LocalOutBuffer,"]");
    
    //version
    char version_str[10];
    snprintf(version_str,10,"%d",fs->version);
    strcat(LocalOutBuffer,"[");
    strcat(LocalOutBuffer,version_str);
    strcat(LocalOutBuffer,"]");
    
    //Send the message to the origin server
    //printf("Sending VLQ to %s %s\n",fs->OriginIP,fs->OriginPort);
    sendMessage(fs->OriginIP,fs->OriginPort,LocalOutBuffer);
  }
}

void *monitorSourceFilesModification(void * relname)
{
  DIR *dir;
  struct dirent *entry;
  char lastmodtime[40];
  char *name = (char *)relname;
  while(1)
  {
    //Monitor files every 5 seconds
  sleep(4);
  if (!(dir = opendir(name)))
      return;
  if (!(entry = readdir(dir)))
      return;

    do 
    {
      if(!( entry->d_type == DT_DIR || *(entry->d_name + strlen(entry->d_name) -1)=='~'))
      {
	//Find the enty in Filestamp table
	struct filestamp *cur_fstmpEntry = getFilestamp(entry->d_name);
	if(cur_fstmpEntry!=NULL)
	{
	  //Entry found in timestamp table
	  //Get the last modification time of this file
	  strcpy(lastmodtime, getFileModifiedTime("./source/",entry->d_name));
	  
	  //Compare times to check if there is modification
	  if((strcmp(cur_fstmpEntry->lastmodtime,lastmodtime)!=0))
	  {
	    DEBUG_PRINT(("Filestamp with modified time found : %s last modified time %s\n",cur_fstmpEntry->fname, cur_fstmpEntry->lastmodtime));
	    DEBUG_PRINT(("Current modified time : %s \n",lastmodtime));
	  
	    //Update the new modified time
	    strcpy(cur_fstmpEntry->lastmodtime,lastmodtime);
	    
	    //Since there is modification, increase the version number of this file in the file status table
	    struct fileStatus *cur_ftblEntry = getFileStatus(entry->d_name);
	    if(cur_ftblEntry!=NULL)
	    {
	      while(head2->status=='L');
	      
	      //Lock the status table
	      head2->status=='L';
	      
	      //Update the Filestatus by increasing the version number
	      cur_ftblEntry->version = cur_ftblEntry->version +1;
	      DEBUG_PRINT(("Filestatus found version incremented %s %c %s %s %d\n",cur_ftblEntry->fname, cur_ftblEntry->status, cur_ftblEntry->OriginIP, cur_ftblEntry->OriginPort, cur_ftblEntry->version));
	      
	      //Unlock the status table
	      head2->status=='U';
	      
	      //Send the devalidate message if the mode is PUSH
	      if(pull_interval==0)
	       SendDVL(cur_ftblEntry);
	      
	    }
	    else
	    {
	      //control should never come here
	      printf("error occured. file not found in file status table");
	    }
	  }
	}
	else
	{
	  printf("New file found : filename : %s\n",entry->d_name);
	  //This means a new file has been added in the source files
	  //Update the filestamp and filestatus tables
	  
	  struct filestamp *temp1 = head1->next;
	  
	  head1->next = createFileStampEntry(entry->d_name);
	  //struct fileStatus *newnode = createFileStatusEntry(entry->d_name,"127.0.0.1","18000",'O',1);
	  struct fileStatus *newnode = createFileStatusEntry(entry->d_name,MyIP,MyQPort,'O',1);
	  
	  insertNewFileStatus(newnode);
	  head1->next->next = temp1;
	}
	
      }
	    //printf("%s\n", entry->d_name);
    } while (entry = readdir(dir));
    
    closedir(dir);
    if(pull_interval>0)
    {
      //printf("Sending VLQ for each file\n");
      //Send valid query message to the origin
      SendVLQ();
    }
  }
 
    
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

  //Print the file status and file stamps;
  //chck_fileStatus();
  //chck_fileTimeStamp();
  
  //Start the query handler thread
  if(pthread_create(MainThreadsList + 0, NULL, StartQHandler, (void *)RC) != 0)
  {
    printf("--ERROR - Could not start the StartQHandler Thread\n");
    exit(0);
  }
  
  //Start file monitoring
    char relPath[10] = "./source";
   if(pthread_create(MainThreadsList + 1, NULL, monitorSourceFilesModification, (void *) relPath) != 0)
  {
    printf("--ERROR - Could not start the Monitor  files Thread\n");
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
    if(GetUserInPut() == 0) //Commented for testing only
    //if(setUserInput() == 0)
    {
      if((RC = SearchFile()) > 0)
      {
	//Some of the peers were not reachable, no action is taken
      }
      else if(RC < 0)
      {
	//Abandoning this search because of the problems or user request
	if(RC!=-7)
	printf("--Could not locate the file \"%s\", RC was %d\n",InFileName, RC);
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
	  //Print the file status and file stamps;

	  //chck_fileStatus();
	  //chck_fileTimeStamp();
  
	}
      }
    }
    //remain in an infinite loop, get the next user input
  }
  //The program control should never come here
  printf("\n***Program is in an inconsistant state, terminating the Node application...\n\t...if this issue persists even after restarting the app and the system, contact the GENIUSES who developed this app***\n");
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