/* This is the main server program. This program is run to activate the iServer (Index Server).
 * Upon start, this server locates and loads the Files List Table (FLT) from file stored in the last run.
 * Files List Table is part of the Linux Kernal, see the Code file FLT.c for the code.
 */
//Header files declaration Section
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
#include <mysql/my_global.h>
#include <mysql/mysql.h>
//Global varibales declaration
//Port number of the MPrt
int MPrt = 40000;
struct sockaddr_in MPrtSocket;
int MPrtSocketNo;
char MessBuff[1000];
char OutBuff[1000];
//Function declarations
void DisplayStartMessage(void);
int StartSocket(int);
void Listen2MPrt(void);
void DisplayCurrentStatus(void);
void HandleRequest(int, struct sockaddr_in);
void NewPeer(void);
void NewFile(void);
void RemoveFile(void);
void ClosePeer(void);
void SearchFile(void);
void SendMessage(int);
//All the below functions return an integer to indicate the status of the operation, the return codes and the corresponding status are as below:
// 0 Success; -1 Arguments invalid; 1 DB Connection Refused; 2 insert failed; 3 delete operation was unsuccessful; 4 select query was not successful
//Adds a file into the FileList table under the IP listed at ipList[0], the file name is passed through fileName
int AddFile(void);
//Sets the current status of a peer
int SetStatus(char);
//Deletes a file from a peers' list, the IP is listed at ipList[0] and the fiile name is passed through fileName
int DelFile(void);
//Locates the IP's of the systems that have a specific file, the file name is passed through fileName and the IP list is returned through ipList[0]
int LocateFile(void);
//An internal  function used by the above functions to connect to the DB
int ConnectDB(void);
//Global variables, defined in Table_Functions
char *ipList[10];
char *fileName;
MYSQL *con;
MYSQL_RES *result;
int DB_NoOfRows;
MYSQL_ROW row;
//Function Definitions start here
void DisplayStartMessage(void)
{
  printf("\n-----------------------------------------\n");
  printf("----------Index Server----------\n");
  return;
}
void DisplayCurrentStatus(void)
{
  printf("\n-----------------------------------------\n");
  printf("Current Status: All is well\n");
  printf("Enter\t \"R\" to Refresh the status\t \"E\" to close the server\n");
  return;
}
//This function creates a socket for the port PrtNo and returns 0 if successfull. 
int StartSocket(int PrtNo)
{
  //Create socket
  int TempInt1;
  if((MPrtSocketNo = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP)) < 0)
  {
    return -1;
  }
  //Bind the port MPrt to the socket created
  memset(&MPrtSocket, 0, sizeof(MPrtSocket));
  MPrtSocket.sin_family = AF_INET;
  MPrtSocket.sin_addr.s_addr = htonl(INADDR_ANY);
  MPrtSocket.sin_port = htons(PrtNo);
  if((TempInt1 = bind(MPrtSocketNo, (struct sockaddr *) &MPrtSocket, sizeof(MPrtSocket))) < 0)
  {
    return -2;
  }
}
//This function is run by the child process, that listens to the MPrt for new messages.
//New processes are created to handle the commands in the messages
void Listen2MPrt(void)
{
  int NewConnSocketNo;
  int TempInt1;
  struct sockaddr_in NewConnSocket;
  //Listen for messages on MPrt for ever
  while(1)
  {
    //Listen for new Connections
    if(listen(MPrtSocketNo, 10) < 0)
    {
      printf("--------Server in Unstable Condition, close the Server by keying Ctrl C---------\n");
      exit(0);
      //continue;
    }
    TempInt1 = sizeof(NewConnSocket);
    if((NewConnSocketNo = accept(MPrtSocketNo, (struct sockaddr *) &NewConnSocket, &TempInt1)) < 0)
    {
      continue;
    }
    if((TempInt1 = fork()) == 0)
    {
      HandleRequest(NewConnSocketNo, NewConnSocket);
      exit(0);
    }
  }
}
//This function accepts messages from the incoming connections and calls appropriate functions to handle the requests
void HandleRequest(int NewConnSocketNo, struct sockaddr_in NewConnSocket)
{
  char MessComm[4];
  int TempInt1;
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
  if(strcmp("SRT", MessComm) == 0)
  {
    NewPeer();
  }
  else if(strcmp("ADD", MessComm) == 0)
  {
    NewFile();
  }
  else if(strcmp("REM", MessComm) == 0)
  {
    RemoveFile();
  }
  else if(strcmp("CLO", MessComm) == 0)
  {
    ClosePeer();
  }
  else if(strcmp("SRC", MessComm) == 0)
  {
    SearchFile();
    SendMessage(NewConnSocketNo);
  }
  else
  {
    //continue;
  }
  close(NewConnSocketNo);
  exit(0);
}
//This function handles the connection requests for new nodes, nodes that want to join the network.
void NewPeer(void)
{
  char SysIP[30];
  int TempInt1, TempInt2;
  for(TempInt1 = 0, TempInt2 = 5; TempInt1 < 25 && MessBuff[TempInt2] != ']'; TempInt1++, TempInt2++)
    SysIP[TempInt1] = MessBuff[TempInt2];
  SysIP[TempInt1] = '\0';
  ipList[0] = (char *)malloc((strlen(SysIP) + 1)*(sizeof(char)));
  strcpy(ipList[0], SysIP);
  TempInt1 = SetStatus('A');
  exit (0);
}
//This function adds new file under an IP in the FileList table
void NewFile(void)
{
  char SysIP[30], InFileName[40];
  int TempInt1, TempInt2;
  for(TempInt1 = 0, TempInt2 = 5; TempInt1 < 25 && MessBuff[TempInt2] != ']'; TempInt1++, TempInt2++)
    SysIP[TempInt1] = MessBuff[TempInt2];
  SysIP[TempInt1] = '\0';
  ipList[0] = (char *)malloc((strlen(SysIP) + 1)*(sizeof(char)));
  strcpy(ipList[0], SysIP);
  for(TempInt1 = 0, TempInt2 += 2; TempInt1 < 40 && MessBuff[TempInt2] != ']'; TempInt1++, TempInt2++)
    InFileName[TempInt1] = MessBuff[TempInt2];
  InFileName[TempInt1] = '\0';
  fileName = (char *)malloc((strlen(InFileName) + 1)*(sizeof(char)));
  strcpy(fileName, InFileName);
  TempInt1 = AddFile();
  exit(0);
}
//This function removes a file from the fileList able for a certain IP
void RemoveFile(void)
{
  char SysIP[30], InFileName[40];
  int TempInt1, TempInt2;
  for(TempInt1 = 0, TempInt2 = 5; TempInt1 < 25 && MessBuff[TempInt2] != ']'; TempInt1++, TempInt2++)
    SysIP[TempInt1] = MessBuff[TempInt2];
  SysIP[TempInt1] = '\0';
  ipList[0] = (char *)malloc((strlen(SysIP) + 1)*(sizeof(char)));
  strcpy(ipList[0], SysIP);
  for(TempInt1 = 0, TempInt2 += 2; TempInt1 < 40 && MessBuff[TempInt2] != ']'; TempInt1++, TempInt2++)
    InFileName[TempInt1] = MessBuff[TempInt2];
  InFileName[TempInt1] = '\0';
  fileName = (char *)malloc((strlen(InFileName) + 1)*(sizeof(char)));
  strcpy(fileName, InFileName);
  TempInt1 = DelFile();
  exit(0);
}
//This function removes a peer from the SystemIndex table and removes all files belonging to the peer from the FileList table
void ClosePeer(void)
{
  char SysIP[30];
  int TempInt1, TempInt2;
  for(TempInt1 = 0, TempInt2 = 5; TempInt1 < 25 && MessBuff[TempInt2] != ']'; TempInt1++, TempInt2++)
    SysIP[TempInt1] = MessBuff[TempInt2];
  SysIP[TempInt1] = '\0';
  ipList[0] = (char *)malloc((strlen(SysIP) + 1)*(sizeof(char)));
  strcpy(ipList[0], SysIP);
  TempInt1 = SetStatus('C');
  exit (0);
}
//This function locates the possible sources for a file and returns IP's of five such peers
void SearchFile(void)
{
  char InFileName[40];
  int TempInt1, TempInt2;
  for(TempInt1 = 0, TempInt2 = 5; TempInt1 < 25 && MessBuff[TempInt2] != ']'; TempInt1++, TempInt2++)
    InFileName[TempInt1] = MessBuff[TempInt2];
  InFileName[TempInt1] = '\0';
  fileName = (char *)malloc((strlen(InFileName) + 1)*(sizeof(char)));
  strcpy(fileName, InFileName);
  if((TempInt1 = LocateFile()) == 0)
  {
    strcpy(OutBuff,"SRCHRES [");
    for(TempInt1 = 0; TempInt1 < DB_NoOfRows; TempInt1++)
    {
      strcat(OutBuff, ipList[TempInt1]);
      strcat(OutBuff, "::");
    }
    strcat(OutBuff, "]");
  }
  else
  {
    strcpy(OutBuff,"SRCHRES []");
  }
}
//This function sends a message to a socket, includes a retry logic for failure to transmit
void SendMessage(int NewConnSocketNo)
{
  int Try = 1, TempInt1 = 0;
  while(Try)
  {
    Try = 0;
    if(send(NewConnSocketNo, OutBuff, strlen(OutBuff), 0) == -1)
    {
      if(TempInt1++ < 5)
      {
	Try = 1;	
      }
    }
  }
}
//This function is called to add new file under an IP
int AddFile(void)
{
  int i;
  char SQLCommand[512];
  if(ipList[0] == NULL || fileName == NULL || strlen(ipList[0]) == 0 || strlen(fileName) == 0)
  {
    return -1;
  }
  if((i = ConnectDB()) != 0)
  {
    return i;
  }
  snprintf(SQLCommand, 512, "INSERT INTO FileList VALUES('%s','%s')",fileName,ipList[0]);
  //printf("------Add - SQL Command: %s \n",SQLCommand);
  if (mysql_query(con, SQLCommand))
  {
      //printf("----------SQL Error Messsage: %s\n", mysql_error(con));
      mysql_close(con);
      return 2;
  }
  mysql_close(con);
  return 0;
}
//Called when a start or close request is received from a system. The valid status values are 'A' and 'C', the program returns an error for other values.
//If a termination si requested, all the files for that system in the FileList table are deleted.
int SetStatus(char Status)
{
  int i;
  char SQLCommand[512];
  if(ipList[0] == NULL || strlen(ipList[0]) == 0 || ((Status != 'A') && (Status != 'C')))
  {
    return -1;
  }
  if((i = ConnectDB()) != 0)
  {
    return i;
  }
  if(Status == 'A')
  {
    snprintf(SQLCommand, 512, "INSERT INTO SystemIndex VALUES('%s','A')",ipList[0]);
    if (mysql_query(con, SQLCommand))
    {
	mysql_close(con);
	return 2;
    }
  }
  else
  {
    snprintf(SQLCommand, 512, "DELETE FROM SystemIndex WHERE SysIP = '%s'",ipList[0]);
    if (mysql_query(con, SQLCommand))
    {
	mysql_close(con);
	return 3;
    }
    snprintf(SQLCommand, 512, "DELETE FROM FileList WHERE SysIP = '%s'",ipList[0]);
    if (mysql_query(con, SQLCommand))
    {
	mysql_close(con);
	return 3;
    }
  }
  mysql_close(con);
  return 0;
}
int DelFile(void)
{
  int i;
  char SQLCommand[512];
  if(ipList[0] == NULL || fileName == NULL || strlen(ipList[0]) == 0 || strlen(fileName) == 0)
  {
    return -1;
  }
  if((i = ConnectDB()) != 0)
  {
    return i;
  }
  snprintf(SQLCommand, 512, "DELETE FROM FileList WHERE SysIP = '%s'AND FileName = '%s'",ipList[0], fileName);
  if (mysql_query(con, SQLCommand))
  {
      mysql_close(con);
      return 3;
  }
  mysql_close(con);
  return 0;
} 
int LocateFile(void)
{
  int i;
  int j;
  char SQLCommand[512];
  //printf("--------File Name: %s\n",fileName);
  if(fileName == NULL || strlen(fileName) == 0)
  {
    return -1;
  }
  if((i = ConnectDB()) != 0)
  {
    return i;
  }
  snprintf(SQLCommand, 512, "SELECT SysIP FROM FileList WHERE FileName = '%s'", fileName);
  if (mysql_query(con, SQLCommand))
  {
    mysql_close(con);
    return 4;
  }
  result = mysql_store_result(con);
  if (result == NULL) 
  {
    mysql_close(con);
    return 4;
  }
  DB_NoOfRows = mysql_num_fields(result);
  
  //printf("--------SQL call result No of Rows: %d\n", DB_NoOfRows);
  
  while((row = mysql_fetch_row(result)))
  {
    for (i=0, j=0; i < DB_NoOfRows && j < 5; i++)
    {
      if (row[i])
      {
	ipList[j] = (char *)malloc((strlen(row[i]) + 1)*sizeof(char));
	strcpy(ipList[j], row[i]);
	j++;
      }
    }
  }
  DB_NoOfRows = j;
  mysql_free_result(result);
  mysql_close(con);
  return 0;
}
int ConnectDB(void)
{
  con = mysql_init(NULL);
  //Connect to MySQL on the system
  if (con == NULL) 
  {
      return(1);
  }
  //Logon to the MySQL server as root user
  if (mysql_real_connect(con, "localhost", "root", "root123", 
          NULL, 0, NULL, 0) == NULL) 
  {
      return(1);
  }
  //Logon to the MySQL server as root user
  if (mysql_query(con, "USE iBase")) 
  {
      return(1);
  }
  return 0;
}
//Main function of the iServer application.
/* This is the main process (MProc) of the iServer Application. MProc listens to the default input port (MPrt)
 * and spawns a new process for each message received. This server does not retain any state information. 
 * This applications provides a command line user interface that can be used to see the number of active connections
 * and transactions. The user command line interface can be used to close the server if needed.
 */
int main()
{
  //Variable declaration
  int MProc;
  int TempInt1;
  char uOption;
  //Main process
  DisplayStartMessage();
  //Create a socket for MPort, report error and exit if the socket was not created.
  if((TempInt1 = StartSocket(MPrt)) != 0)
  {
    printf("---Server Error: Unable to create a socket for port %d, change the port number and retry\n",MPrt);
    return(0);
  }
  if((MProc = fork()) == 0)
  {
    Listen2MPrt();
  }
  while(1)
  {
    DisplayCurrentStatus();
    printf("Enter Option >> ");
    //Read user option
    uOption = getchar();
    //Perform the action
    switch (uOption)
    {
      case 'r':
      case 'R':
	DisplayCurrentStatus();
	break;
     //Exit if requested
      case 'e':
      case 'E':
	printf("Server closing, issueing kill signal...\n");
	//kill(MProc);
	printf("All processes terminated\nServer was successfully shutdown.\n");
	//return(0);
	break;
      default:
	printf("Option \"%c\" is not valid.\nEnter\t \"R\" to Refresh the status\t \"E\" to close the server\n",uOption);
	break;
    }
  }
  //The program execution should never come here.
  printf("\nERROR: Program is in an unstable state, the server will terminate.\n");
  //kill(MProc);
  return(1);
}
//End of Programme