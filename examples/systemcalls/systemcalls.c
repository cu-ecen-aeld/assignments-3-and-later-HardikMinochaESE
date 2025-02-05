#include "systemcalls.h"
#include <syslog.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>

/**
 * @param cmd the command to execute with system()
 * @return true if the command in @param cmd was executed
 *   successfully using the system() call, false if an error occurred,
 *   either in invocation of the system() call, or if a non-zero return
 *   value was returned by the command issued in @param cmd.
*/
bool do_system(const char *cmd)
{

/*
 * TODO  add your code here
 *  Call the system() function with the command set in the cmd
 *   and return a boolean true if the system() call completed with success
 *   or false() if it returned a failure
*/

    if(cmd == NULL){
    
	    syslog(LOG_DEBUG, "Invalid command\n");
	    return false;
    
    }
    
    bool success = (system(cmd)==0)?true:false;

    return(success);

}

/**
* @param count -The numbers of variables passed to the function. The variables are command to execute.
*   followed by arguments to pass to the command
*   Since exec() does not perform path expansion, the command to execute needs
*   to be an absolute path.
* @param ... - A list of 1 or more arguments after the @param count argument.
*   The first is always the full path to the command to execute with execv()
*   The remaining arguments are a list of arguments to pass to the command in execv()
* @return true if the command @param ... with arguments @param arguments were executed successfully
*   using the execv() call, false if an error occurred, either in invocation of the
*   fork, waitpid, or execv() command, or if a non-zero return value was returned
*   by the command issued in @param arguments with the specified arguments.
*/

bool do_exec(int count, ...)
{
    va_list args;
    va_start(args, count);
    char * command[count+1];
    int i;
    for(i=0; i<count; i++)
    {
        command[i] = va_arg(args, char *);
    }
    command[count] = NULL;
    // this line is to avoid a compile warning before your implementation is complete
    // and may be removed
    // command[count] = command[count];

/*
 * TODO:
 *   Execute a system command by calling fork, execv(),
 *   and wait instead of system (see LSP page 161).
 *   Use the command[0] as the full path to the command to execute
 *   (first argument to execv), and use the remaining arguments
 *   as second argument to the execv() command.
 *
*/
    openlog(NULL, 0, LOG_USER);
    
    // Fork the Process
    int child_pid = fork();
    
    // Check if fork was successful. If not raise an error.
    if(child_pid == -1){
        syslog(LOG_ERR, "Failed to fork process....\n");
	return(false);
    }
    if(child_pid == 0){
	    // If fork() was successful, try execv() command.
	    if(execv(command[0], command)){
		    syslog(LOG_ERR, "Failed to execute system process\n");
		    exit(1); 
	    }
    }
    // if execv() doesn't return, wait for the child process to finish executing.
    else{

	int return_status;
	
	// wait for child process to finish normally, and save the return status in "return_status" variable.
        waitpid(child_pid,&return_status, 0);

        if(return_status != 0){
            syslog(LOG_ERR, "Execution for pid:%d failed with return code: %d\n", child_pid, return_status);
            return(false);
        }

        syslog(LOG_DEBUG, "Process execution for pid:%d completed successfuly\n",child_pid);

    }
    
    closelog();
    
    va_end(args);

    return true;
}

/**
* @param outputfile - The full path to the file to write with command output.
*   This file will be closed at completion of the function call.
* All other parameters, see do_exec above
*/
bool do_exec_redirect(const char *outputfile, int count, ...)
{
    va_list args;
    va_start(args, count);
    char * command[count+1];
    int i;
    for(i=0; i<count; i++)
    {
        command[i] = va_arg(args, char *);
    }
    command[count] = NULL;
    // this line is to avoid a compile warning before your implementation is complete
    // and may be removed
    //command[count] = command[count];


/*
 * TODO
 *   Call execv, but first using https://stackoverflow.com/a/13784315/1446624 as a refernce,
 *   redirect standard out to a file specified by outputfile.
 *   The rest of the behaviour is same as do_exec()
 *
*/

    openlog(NULL, 0, LOG_USER);
    
    // Open a file descriptor to redirect the standard out.
    int fd = open(outputfile, (O_WRONLY | O_CREAT), 0644);
    
    if(fd < 0){
        syslog(LOG_ERR, "Could not open/create outputfile...\n");
        return(false);       
    }
    
    // Fork the Process
    int child_pid = fork();
    
    // Check if fork was successful. If not raise an error.
    if(child_pid == -1){
        syslog(LOG_ERR, "Failed to fork process....\n");
	return(false);
    }

    if(child_pid == 0){
		// Try to redirect the standard out to outputfile 
	    if (dup2(fd, 1) < 0){
		syslog(LOG_ERR, "Could not redirect STDOUT to file...\n");
		return(false);
	    }
	    
	    close(fd);
	    
	    // If fork() was successful, try execv() command.
	    if(execv(command[0], command)){
		    syslog(LOG_ERR, "Failed to execute system process\n");
		    exit(1); 
	    }
    }

    // if execv() doesn't return, wait for the child process to finish executing.
    else{

	int return_status;
	
	// wait for child process to finish normally, and save the return status in "return_status" variable.
        waitpid(child_pid,&return_status,0);

        if(return_status != 0){
            syslog(LOG_ERR, "Execution for pid:%d failed with return code: %d\n", child_pid, return_status);
            return(false);
        }

        syslog(LOG_DEBUG, "Process execution for pid:%d completed successfuly\n",child_pid);

    }
    
    closelog();


    va_end(args);

    return true;
}
