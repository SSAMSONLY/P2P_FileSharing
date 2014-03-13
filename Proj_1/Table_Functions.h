#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <mysql/my_global.h>
#include <mysql/mysql.h>
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