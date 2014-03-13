#include <unistd.h>
#include <sys/types.h>
#include <dirent.h>
#include <stdio.h>

#include<sys/socket.h>
#include<arpa/inet.h>
#include<netinet/in.h>
#include<signal.h>
#include <pthread.h>
#include <time.h>
#include <sys/stat.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>

extern char PeerIP[20][20], PeerPort[20][20];
extern int NeighbourCount;

struct filestamp
{
  char fname[50];
  char lastmodtime[40];
  struct filestamp *next;
};

struct fileStatus
{
  char fname[50];
  char status;
  char OriginIP[21];
  char OriginPort[6];
  int version;
  struct fileStatus *next;
};

  struct filestamp *head1 = NULL;
  struct fileStatus *head2 = NULL;
  
  struct fileStatus * createFileStatusEntry(char * fname, char * IP, char * port, char status, int version);
  struct filestamp * createFileStampEntry(char * fname);
  struct filestamp * getFilestamp (char* filename);
  struct fileStatus * getFileStatus (char* filename);
  char * getFileModifiedTime(char * relPath, char * filename);
  void *monitorSourceFilesModification(void * relname);
  void insertNewFileStatus(struct fileStatus * newnode);
  void chck_fileStatus();
  void chck_fileTimeStamp();
  
//function to retrieve the list of files from a given directory recursively
void loadSourceFiles(const char *name, char * OriginIP, char * OriginPort)
{
    DIR *dir;
    struct dirent *entry;
    struct stat statbuf;

    
    head1 = (struct filestamp *)malloc(sizeof(struct filestamp));
    head2 = (struct fileStatus *)malloc(sizeof(struct fileStatus));
    head2 -> next = NULL;
    //Set the file status list as unlocked for future multithread access
    head2->status = 'U';	  
    
    struct filestamp *current_filestampentry = head1;
    struct fileStatus *current_filetableentry = head2;
  
    if (!(dir = opendir(name)))
        return;
    if (!(entry = readdir(dir)))
        return;

    do 
    {
      //printf("CKP----1\n");
      //Ignore directories and files ending with ~
      if(!( entry->d_type == DT_DIR || *(entry->d_name + strlen(entry->d_name) -1)=='~' ))
      {
	//Create file stamp entry
        current_filestampentry->next = (struct filestamp *)createFileStampEntry(entry->d_name);
	current_filestampentry = current_filestampentry->next;
	//printf("CKP----5\n");

	//Create File status entry
	current_filetableentry->next = (struct fileStatus *)createFileStatusEntry(entry->d_name,OriginIP,OriginPort,'O',1);
	current_filetableentry = current_filetableentry->next;
      }
	
            //printf("%s\n", entry->d_name);
    } while (entry = readdir(dir));
    closedir(dir);
 
    
}

struct filestamp * createFileStampEntry(char * fname)
{
  struct filestamp *current_filestampentry = (struct filestamp *)malloc(sizeof(struct filestamp));;
  struct stat statbuf;
  
  //printf("CKP----2\n");
  
  strcpy(current_filestampentry->fname , fname);

  strcpy(current_filestampentry->lastmodtime, (char*)getFileModifiedTime("./source/",fname));
  
  current_filestampentry->next = NULL;
  //printf("CKP----4\n");
  return current_filestampentry;
}

struct fileStatus * createFileStatusEntry(char * fname, char * IP, char * port, char status, int version)
{
  struct fileStatus *current_filetableentry = (struct fileStatus *)malloc(sizeof(struct fileStatus));;
  
  strcpy(current_filetableentry->fname , fname);
    
  current_filetableentry->status = status;
  current_filetableentry->version = version;
  
  strcpy(current_filetableentry->OriginIP, IP);
  strcpy(current_filetableentry->OriginPort, port);
  

  current_filetableentry->next = NULL;
  
  return current_filetableentry;
}

struct filestamp * getFilestamp (char* filename)
{
  struct filestamp *cur_fstmpEntry = head1;
  while(cur_fstmpEntry->next!=NULL)
  {
    cur_fstmpEntry=cur_fstmpEntry->next;
    
    if(strcmp(cur_fstmpEntry->fname,filename)==0)
    {
      return cur_fstmpEntry;
    }
  }
  return NULL;
}

struct fileStatus * getFileStatus (char* filename)
{
  struct fileStatus *cur_fsttspEntry = head2;
  while(head2->status=='L');
	      
  //Lock the status table
  head2->status=='L';
  while(cur_fsttspEntry->next!=NULL)
  {
    cur_fsttspEntry=cur_fsttspEntry->next;
    
    if(strcmp(cur_fsttspEntry->fname,filename)==0)
    {
      head2->status=='U';
      return cur_fsttspEntry;
    }
  }
  head2->status=='U';
  return NULL;
}

//Gives the last modified time of the file in format [Sat Oct  5 19:12:34 2013]
char * getFileModifiedTime(char * relPath, char * filename)
{
  struct stat statbuf;
  char filepath[50];
  strcpy(filepath, relPath);
  if(!(char*)(relPath + strlen(relPath) - 1)=='/')
  {
    strcat(filepath, "/");
  }

  strcat(filepath,filename);
  //if the modified time is increased then increase version in the filestatus table
  stat(filepath,&statbuf);//ctime( &statbuf.st_mtime)
  return ctime(&statbuf.st_mtime);
}

void dvldtFile(struct fileStatus * dvlfl)
{
  while(head2->status=='L');
	      
  //Lock the status table
  head2->status=='L';
  
  dvlfl->status ='I';
  //Unlock the status table
  head2->status=='U';
}


void insertNewFileStatus(struct fileStatus * newnode)
{
  //check if there is already a file entry with the same name
  struct fileStatus *fs = getFileStatus(newnode->fname);
  if(fs!=NULL)
  {
    //since there exists a file with the same name, we need to replace it
    while(head2->status=='L');
		
    //Lock the status table
    head2->status=='L';
    
    strcpy(fs->fname,newnode->fname);
    strcpy(fs->OriginIP,newnode->OriginIP);
    strcpy(fs->OriginPort,newnode->OriginPort);
    fs->version = newnode->version;
    //Update status if its is invalid 
    if(fs->status == 'I')
      fs->status = 'V';
    //Unlock the status table
    head2->status=='U';
  }
  else
    
  {
    //since there does not exists a file with the same name, we need to replace it
      while(head2->status=='L');
	      
  //Lock the status table
  head2->status=='L';
  
  struct fileStatus *temp2 = head2->next;

  head2->next = newnode;
  head2->next->next = temp2;
  
  //Unlock the status table
  head2->status=='U';
  }

  
}

void chck_fileTimeStamp()
{
  struct filestamp *temp1 = head1;

  temp1=temp1->next;
  
  if(temp1==NULL)
    printf("\n Master files : None found\n");
  else
    printf("\n File Time Stamp table : \n");
  while(temp1!=NULL)
  {
    printf(" %s \t %s",temp1->fname, temp1->lastmodtime);
    temp1=temp1->next;  
  } 
}

void chck_fileStatus()
{

  struct fileStatus *temp2 = head2;
  
  temp2=temp2->next;
  if(temp2==NULL)
    printf("\n Downloaded files : None found");
  else
  printf("\n File Status  Table : \n");
  
  while(temp2!=NULL)
  {
    printf(" %s \t %c \t %s \t %s \t %d\n",temp2->fname, temp2->status, temp2->OriginIP, temp2->OriginPort, temp2->version);
    temp2=temp2->next;
  } 
}

void modifyfile(char * relPath, char * filename)
{
  char filepath[50];
  strcpy(filepath, relPath);
  if(!(char*)(relPath + strlen(relPath) - 1)=='/')
  {
    strcat(filepath, "/");
  }

  strcat(filepath,filename);
  
  FILE *fp;
  if((fp = fopen(filepath, "a")) == NULL)
  {
    return;
  }
  //Append to end of file
  fseek(fp, 0L, SEEK_END);
  fputc('Z',fp);
  fclose(fp);
}

void showPeers(void)
{
  int i;
  printf("--All Peers\n");
  for (i = 0; i < NeighbourCount; i ++)
  {
    //Show all peers from Config File
    printf("\t%s:%s\n",PeerIP[i],PeerPort[i]);
  }
}
/*
int main()
{
  pthread_t MainThreadsList[10];
  loadSourceFiles("./source");
    
  
  chck_fileStatus();
  chck_fileTimeStamp();
  
  //modify files here
 // sleep(15);
  modifyfile("./source/","text_1kb.txt");
  
  char relPath[10] = "./source";
  //Start the query handler thread
  if(pthread_create(MainThreadsList + 0, NULL, monitorSourceFilesModification, (void *) relPath) != 0)
  {
    printf("--ERROR - Could not start the Monitor  files Thread\n");
    exit(0);
  }
  
  while(1)
  {
  chck_fileStatus();
  chck_fileTimeStamp();
  modifyfile("./source/","text_2kb.txt");
    sleep(6);

  }

  //monitorSourceFilesModification("./source");
  

  
  printf(":::::::::::::::::::::::::::::::::::::::::::\n");

 
  sleep(1);
  free(head1);
  free(head2);
  
  
}
*/