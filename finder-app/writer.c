/**
 * Compiled on Virtual Box using Makefile
 * Compiler Flags Used: -Wall & -Werror
 * @file    writer.c
 * @brief   Assignment 2 of course ECEN-5713 (AESD)
 * @author  Aamir Suhail Burhan
 * @version 1.0
 * @Submission Date: 10th September 2023
 *
 * @description  The writer.c application creates a textfile in a specified path with a 
 * string text passed as an argument.
 *
 */

/***************************************************HEADER FILES***********************************/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <syslog.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#define ERROR (-1)

/* Application entry point: main */
int main(int argc, char *argv[])
{
	openlog("writer", LOG_CONS | LOG_PID, LOG_USER);

	if (argc != 3)
	{
		syslog(LOG_ERR, "Correct Usage: writer <Path> <Text String>");
		closelog();
		return 1;	// Failure
	}

	const char *writefile = argv[1];	// Extract directory path 
	const char *writestr = argv[2];	// Extract text string to be written

	int fd = open(writefile, O_WRONLY | O_CREAT | O_TRUNC, S_IRWXU);
	if (fd == ERROR)
	{
		syslog(LOG_ERR, "Error in opening file: %s", strerror(errno));
		closelog();
		return 1;	// Failure
	}

	ssize_t bytes_written = write(fd, writestr, strlen(writestr));
	if (bytes_written == ERROR)
	{
		syslog(LOG_ERR, " Error in writing to file: %s", strerror(errno));
		close(fd);
		closelog();
		return 1; 	// Failure
	}

	close(fd);
	syslog(LOG_DEBUG, "Writing %s to %s", writestr, writefile);
	closelog();
	return 0;	// Success
}

