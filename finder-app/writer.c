/*
*    File: writer.c
*    File Created: 25 Jan 2025
*    Author: Hardik Minocha
*    
*    This writer utility takes command line arguments when invoked
*    Usage:
*    
*    witer.o <path/to/file> <string to write> 
*    
*    
*/

#include <stdio.h>
#include <stdlib.h>
#include <syslog.h>

int main(int argc, char *argv[]){
	
	// Setting up SysLogger as LOG_USER
	openlog(NULL, 0, LOG_USER);
	
	if(argc != 3){
		syslog(LOG_ERR, "Call Error: Usage \r\n writer: <writefile>, <writestr>\r\n");
		closelog();
		return(1);
	}
	
	const char *existing_file = argv[1];
	const char *writestr = argv[2];
	
	FILE *writefile = fopen(existing_file, "w");
	if (existing_file == NULL){
		syslog(LOG_ERR, "Execution Error: Failed to open file. Please check if the file path is correct.\r\n");
		closelog();
		return(1);
	}
	
	syslog(LOG_DEBUG, "Writing %s to %s", writestr, existing_file);
	
	if(fputs(writestr, writefile) == -1){
		syslog(LOG_ERR, "Execution Error: Could not write to the file.\r\n");
		fclose(writefile);
		closelog();
		return(1);
	}
	
	syslog(LOG_DEBUG, "Succesfully wrote %s to %s", writestr, existing_file);
	
	return(0);
}

