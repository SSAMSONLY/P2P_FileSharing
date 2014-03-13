/* This is program that is run one off to setup the DB and Tables required by the Index Server (iServer).
 * iServer uses a Database - iBase (Index Base) and has two tables - FileList and SystemIndex tables.
 * FileList table has two Columns - FileName (text, not null) and SysIP (varchar(15), not null).
 * SystemIndex table also has two columns - SysIP (varchar(15), not null) and Status (char, not null).
 * This program is run once, during the iServer installation.
 */
#include <stdio.h>
#include <mysql/my_global.h>
#include <mysql/mysql.h>
int main()
{  
  MYSQL *con = mysql_init(NULL);

  //Connect to MySQL on the system
  if (con == NULL) 
  {
      printf("Connection to MySQL failed, system error message: %s\n", mysql_error(con));
      exit(1);
  }
  //Logon to the MySQL server as root user
  if (mysql_real_connect(con, "localhost", "root", "root123", 
          NULL, 0, NULL, 0) == NULL) 
  {
      printf("Could not logon to MySQL, system error message: %s\n", mysql_error(con));
      mysql_close(con);
      exit(1);
  }
  //Create the Database
  if (mysql_query(con, "CREATE DATABASE iBase")) 
  {
      printf("Could not create the DB iBase, system error message: %s\n", mysql_error(con));
      mysql_close(con);
      exit(1);
  }
  //Using iBase as the DB for current operations
  if (mysql_query(con, "use iBase")) 
  {
      printf("Could not connect to the DB iBase, system error message: %s\n", mysql_error(con));
      mysql_close(con);
      exit(1);
  }
  printf("--Connected to the Database iBase\n");
  //Create table FileList
  if (mysql_query(con, "CREATE TABLE FileList(FileName TEXT, SysIP TEXT)")) 
  {
      printf("Could not create the table FileList, system error message: %s\n", mysql_error(con));
      mysql_close(con);
      exit(1);
  }
  printf("--Table FileList created\n");
  //Create table SystemIndex
  if (mysql_query(con, "CREATE TABLE SystemIndex(SysIP TEXT, Status CHAR)")) 
  {
      printf("Could not create the table SystemIndex, system error message: %s\n", mysql_error(con));
      mysql_close(con);
      exit(1);
  }
  printf("--Table SystemIndex created\n");
  printf("iServer tables ready for use\n");
  mysql_close(con);
  exit(0);
}