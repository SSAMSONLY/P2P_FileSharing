#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <time.h>
 
void ProcessConn(int, int);
int main(int argc, char *argv[])
{
   //Declaring process variables.
   int server_sockfd, client_sockfd;
   int server_len, client_len;
   int ch = 1, pid;
   struct sockaddr_in server_address;
   struct sockaddr_in client_address;
 
   //Remove any old socket and create an unnamed socket for the server.
   server_sockfd = socket(AF_INET, SOCK_STREAM, 0);
   server_address.sin_family = AF_INET;
   server_address.sin_addr.s_addr = htons(INADDR_ANY);
   server_address.sin_port = htons(17000);
   server_len = sizeof(server_address);
 
   bind(server_sockfd, (struct sockaddr *) &server_address, server_len);
 
   //Create a connection queue and wait for clients
   listen(server_sockfd, 5);
   while(1)
   {
      printf("Server Main Waiting\n");
 
      //Accept a connection
      client_len = sizeof(client_address);
      client_sockfd = accept(server_sockfd, (struct sockaddr *) &client_address, &client_len);
 
      //Read write to client on client_sockfd
      if((pid = fork()) == 0)
      {
	pid = getpid();
	printf("--In Child process: %d\n",pid);
	ProcessConn(client_sockfd, ch);
	printf("--Process %d exiting\n", pid);
	exit(0);
      }
       ch++;
   }
 
   return 0;
}
void ProcessConn(int client_sockfd, int ch)
{
  int len, TempInt1 = 0, CurrSeg = 1, TempInt2 = 0, TempInt3;
  long FileSize, FileByteCounter = 0;
  char buffer[100], fch, OutBuffer[1000], TempStr[100];
  FILE *fp;
  len = read(client_sockfd, buffer, 100);
  buffer[len] = '\0';
  printf("--Message from Client %d: %s\n",ch,buffer);
  if((fp = fopen(buffer,"r")) == NULL)
  {
    printf("Could not open the file \"%s\"\n",buffer);
    exit(0);
  }
  len = 0;
  fseek(fp, 0L, SEEK_END);
  FileSize = len = ftell(fp);
  printf("--Size of File: %d\n",len);
  if((len%500) == 0)
    len = len/500;
  else
    len = len/500 + 1;
  printf("--No of segments: %d\n", len);
  fseek(fp, 0L, SEEK_SET);
      strcpy(OutBuffer, "FLT|");
      //strcat(OutBuffer, buffer);
      strcat(OutBuffer, "|");
      //itoa(CurrSeg,TempStr,10);
      snprintf(TempStr, 10, "%d", CurrSeg);
      strcat(OutBuffer, TempStr);
      strcat(OutBuffer, ":");
      //itoa(len,TempStr,10);
      snprintf(TempStr, 10, "%d", len);
      strcat(OutBuffer, TempStr);
      strcat(OutBuffer, "[");
      TempInt1 = strlen(OutBuffer);
      printf("--Size of Header: %d\n",TempInt1);
      --TempInt1;
      //printf("--Press any key to continue\n");
      //TempInt2 = getchar();
      TempInt2 = 0;
  while(((fch = fgetc(fp))!= EOF) && (FileByteCounter++ < FileSize))
  {
    if((TempInt1++ == 996) || (TempInt2 >= 499))
    {
      OutBuffer[TempInt1] = fch;
      OutBuffer[TempInt1 + 1] = '\0';
      printf("\n--Sending message of length %d:\n%s\n",TempInt1+1 ,OutBuffer);
      TempInt3 = read(client_sockfd, buffer, 100);
      buffer[TempInt3] = '\0';
      //printf("\n--ACK from Client %s\n",buffer);
      write(client_sockfd, OutBuffer, TempInt1 + 1);
      CurrSeg++;
      strcpy(OutBuffer, "FLT|");
      //strcat(OutBuffer, buffer);
      strcat(OutBuffer, "|");
      //itoa(CurrSeg,TempStr,10);
      snprintf(TempStr, 10, "%d", CurrSeg);
      strcat(OutBuffer, TempStr);
      strcat(OutBuffer, ":");
      //itoa(len,TempStr,10);
      snprintf(TempStr, 10, "%d", len);
      strcat(OutBuffer, TempStr);
      strcat(OutBuffer, "[");
      TempInt1 = strlen(OutBuffer);
      --TempInt1;
      TempInt2 = 0;
    }
    else
    {
      OutBuffer[TempInt1] = fch;
      TempInt2++;
      //printf("Adding Char %d\t:%c\n",TempInt1,fch);
    }
  }
  //OutBuffer[++TempInt1] = fch;
  OutBuffer[TempInt1 + 1] = '\0';
  //printf("\n--Sending message of length %d:\n%s\n",TempInt1 + 1 ,OutBuffer);
  write(client_sockfd, OutBuffer, TempInt1 + 1);
  fclose(fp);
  printf("--File Transfer Complete--\n");
  close(client_sockfd);
}