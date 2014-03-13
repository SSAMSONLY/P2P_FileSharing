/* This file has the set of functions that the called by tye iServer Processes to access the tables.
 * The functions defined in this file provide interface to add, delete, modify and search from the tables.
 */
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <mysql/my_global.h>
#include <mysql/mysql.h>
//Global variables, also defined in Server_Main.c
char *ipList[10];
char *fileName;
MYSQL *con;
MYSQL_RES *result;
int DB_NoOfRows;
MYSQL_ROW row;
//Function Declarations
//All functions return an integer to indicate the status of the operation, the return codes and the corresponding status are as below:
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
//Function definitions
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
//This is a test main function
/*int main(void)
{
  char Op = 'Y', Continue = 'Y';
  char TempStr1[100], TempStr2[100];
  int RC;
  printf("\n-----Options-----\n");
  printf("\tA - Add\tM - Modify\tS - Search\n\tE - Exit\n");
  while(Op != 'E' && Op != 'e')
  {
    printf("\n-----Options-----\n");
    printf("\tA - Add\tM - Modify\tS - Search\n\tE - Exit\nOption >");
    scanf("%c",&Op);
    switch (Op)
    {
      case 'A':
      case 'a':
	printf("\n--Enter IP: ");
	scanf("%s",TempStr1);
	printf("\n--Enter File: ");
	scanf("%s",TempStr2);
	fileName = (char *)malloc((strlen(TempStr2) + 1)*(sizeof(char)));
	strcpy(fileName, TempStr2);
	ipList[0] = (char *)malloc((strlen(TempStr1) + 1)*(sizeof(char)));
	strcpy(ipList[0], TempStr1);
	RC = AddFile();
	printf("\n---Return Code from the Add: %d\n",RC);
	break;
      case 'M':
      case 'm':
	printf("\n--Enter IP: ");
	scanf("%s",TempStr1);
	ipList[0] = (char *)malloc((strlen(TempStr1) + 1)*(sizeof(char)));
	strcpy(ipList[0], TempStr1);
	printf("\n--Enter Status: ");
	//Op = getchar();
	scanf("%c",&Op);
	Op = 'C';
	RC = SetStatus(Op);
	printf("\n---Return Code from Modify: %d\n",RC);
	break;
      case 'S':
      case 's':
	printf("\n--Enter the File: ");
	scanf("%s",TempStr2);
	fileName = (char *)malloc((strlen(TempStr2) + 1)*(sizeof(char)));
	strcpy(fileName, TempStr2);
	RC = LocateFile();
	printf("\n---Return Code from Search: %d\n",RC);
	if(RC == 0)
	{
	  printf("----No of results: %d\n",DB_NoOfRows);
	  for(RC = 0; RC < DB_NoOfRows; RC++)
	  {
	    printf("----IP: %s\n",ipList[RC]);
	  }
	}
	break;
      default:
	break;
    }
  } 
  return 0;
}
*/