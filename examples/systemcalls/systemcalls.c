#include "systemcalls.h"
#include <stdlib.h>
#include <syslog.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <fcntl.h>
#define ERROR (-1)
#define SHELL_EXECUTION_FAILURE_CODE (127)

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
    openlog("systemcalls", LOG_CONS | LOG_PID, LOG_USER);
    if (cmd == NULL)
    {
	syslog(LOG_ERR,"Command is Null");
	closelog();
	return false;
    }

    int status = system(cmd);  // Call system() function for the command 'cmd'
    if (status == ERROR)
    {
	syslog(LOG_ERR,"Child processor could not be created or the its status could not be retrieved",errno);
        closelog();
	return false;
    }	

    // Check if the child was terminated normally
    if (WIFEXITED(status))
    {
	int child_status = WEXITSTATUS(status);  // Check the exit status of the child
	if (child_status == SHELL_EXECUTION_FAILURE_CODE)
	{
            syslog(LOG_ERR,"Shell coudl not be executed in child process");
	    closelog();
	    return false;
	}
    }

    if (status != 0)
    {
	syslog(LOG_ERR,"Command execution failed with status: %d",status);
	closelog();
	return false;
    }

    closelog();
    return true;
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
    pid_t child_pid = fork();

    openlog("systemcalls", LOG_CONS | LOG_PID, LOG_USER);
    if (child_pid == ERROR)
    {
	syslog(LOG_ERR,"Fork Failed");
	va_end(args);
	closelog();
	return false;
    }

    if (child_pid == 0)
    {
	// Control is in Child Process
	if (execv(command[0], command) == ERROR)
	{
	    syslog(LOG_ERR,"Execv failed");
            va_end(args);
	    closelog();
	    exit(EXIT_FAILURE);
	}
    }
    else
    {
	int status;

	if (waitpid(child_pid, &status, 0) == ERROR)
	{
	    syslog(LOG_ERR,"waitpid failed");
            va_end(args);
	    closelog();
	    return false;	    
	}

	// Check if child process exited normally
	if (WIFEXITED(status))
	{
	    int exit_status = WEXITSTATUS(status);
	    if(exit_status == 0)
	    {
		va_end(args);
		closelog();
		return true;
	    }
	    else
	    {
		syslog(LOG_ERR,"Non-zero exit status");
		va_end(args);
		closelog();
		return false;
	    }
	}
	else
	{
	    syslog(LOG_ERR,"Child process didnt exit normally");
	    va_end(args);
	    closelog();
	    return false;
	}
    }
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
   // command[count] = command[count];


/*
 * TODO
 *   Call execv, but first using https://stackoverflow.com/a/13784315/1446624 as a refernce,
 *   redirect standard out to a file specified by outputfile.
 *   The rest of the behaviour is same as do_exec()
 *
*/
    pid_t child_pid = fork();

    openlog("systemcalls", LOG_CONS | LOG_PID, LOG_USER);
    if (child_pid == ERROR)
    {
        syslog(LOG_ERR,"Fork Failed");
        va_end(args);
        closelog();
        return false;
    }

    if (child_pid == 0)
    {
	int fd = open(outputfile, O_WRONLY |O_CREAT |O_TRUNC, 0644);
	if (fd == ERROR)
	{
	    syslog(LOG_ERR,"Opening file failed");
	    va_end(args);
	    exit(EXIT_FAILURE);
	}

	if (dup2(fd, STDOUT_FILENO) == ERROR)
	{
	    syslog(LOG_ERR, "Redirecting failed");
	    va_end(args);
	    close(fd);
	    exit(EXIT_FAILURE);
	}

	close(fd);  // Close file descriptor

        // Control is in Child Process
        if (execv(command[0], command) == ERROR)
        {
            syslog(LOG_ERR,"Execv failed");
            va_end(args);
            closelog();
            exit(EXIT_FAILURE);
        }
    }
    else
    {
        int status;

        if (waitpid(child_pid, &status, 0) == ERROR)
        {
            syslog(LOG_ERR,"waitpid failed");
            va_end(args);
            closelog();
            return false;
        }

        // Check if child process exited normally
        if (WIFEXITED(status))
        {
            int exit_status = WEXITSTATUS(status);
            if(exit_status == 0)
            {
                va_end(args);
                closelog();
                return true;
            }
            else
            {
                syslog(LOG_ERR,"Non-zero exit status");
                va_end(args);
                closelog();
                return false;
            }
        }
        else
        {
            syslog(LOG_ERR,"Child process didnt exit normally");
            va_end(args);
            closelog();
            return false;
        }
    }
}
