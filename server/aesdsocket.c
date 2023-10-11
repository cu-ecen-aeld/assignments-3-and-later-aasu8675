/**
 * Compiled on Virtual Box using Makefile
 * Compiler Flags Used: -Wall & -Werror
 * @file    aesdsocket.c
 * @brief   Assignment 5 of course ECEN-5713 (AESD)
 * @author  Aamir Suhail Burhan
 * @version 1.0
 * @Submission Date: 8th October 2023
 *
 * @description  The aesdsocket.c application is a socket based program which is used
 * to setup a socket and receives data over a connection and appends it to a file.
 *
 * References: https://beej.us/guide/bgnet/html/
 */


/**************************************HEADER FILES*********************************/
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <sys/wait.h>
#include <signal.h>
#include <syslog.h>
#include <fcntl.h>
#include <stdbool.h>


/*****************************************DEFINES***********************************/
#define PORT "9000"
#define ERROR (-1)
#define BACKLOG (10)  // Number of pending connection queue will hold
#define MAX_BUFFER_SIZE 1024
#define DATA_FILE_PATH "/var/tmp/aesdsocketdata"


int server_fd, client_fd;  // File descriptors for server and client
int file_fd;  // Files descriptor for the data file
bool error_flag = false, daemon_flag = false;


/* Description: This function closes all file descriptors and deletes the created 
 * file.
 */
void cleanup(void)
{
	if(close(server_fd) == ERROR)
	{
		perror("Server fd");
		syslog(LOG_ERR,"Server fd close");
	}

	if(close(client_fd) == ERROR)
	{
		perror("Client fd");
		syslog(LOG_ERR, "Client fd close");
	}

	// Error handling for closing fd
	if(close(file_fd) == ERROR)
	{
		perror("File close");
		syslog(LOG_ERR, "Error in closing file");
		error_flag = true;
	}

	// Error handling for deleting file
	if(remove(DATA_FILE_PATH) == ERROR)
	{
		perror("File removal");
		syslog(LOG_ERR, " File removal failed");
	}
	closelog();
}


/* Description: This function provides support to run this application
 * as a daemon
 */
void daemon_mode(void)
{
		pid_t process_id = fork();  // Forking the parent
		if (process_id == ERROR)
		{
			perror("Fork failed");
			syslog(LOG_ERR, "Fork failed");
			exit(1);
		}

		// Valid process id
		if(process_id)
		{
			syslog(LOG_INFO,"Exiting parent process");
			exit(0);
		}

		// Start a new session
		if(setsid() == ERROR)
		{
			perror("setsid");
			syslog(LOG_ERR, "Error in creating daemon session");
			exit(1);
		}

		chdir("/");  // Change directory to rootf

		// Close standard file descriptors
		close(STDIN_FILENO);
		close(STDOUT_FILENO);
		close(STDERR_FILENO);
}


/* Description: This function handles the signals SIGINT and SIGTERM and gracefully 
 * exits the program on receiving them.
 */
void signal_handler(int signo)
{
	if (signo == SIGINT || signo == SIGTERM)
	{
		syslog(LOG_INFO,"Caught signal,exiting");
		cleanup();  // Perform cleanup
	}
}

// get sockaddr, IPv4 or IPv6:
void *get_in_addr(struct sockaddr *sa)
{
	if (sa->sa_family == AF_INET) {
		return &(((struct sockaddr_in*)sa)->sin_addr);
	}

	return &(((struct sockaddr_in6*)sa)->sin6_addr);
}


/**********************************Application Entry*********************************/
int main(int argc, char *argv[])
{
	openlog("aesdsocket", LOG_CONS | LOG_PID, LOG_USER);

	// Check if -d is passed to run this application as a daemon
	if ((argc == 2)  && strcmp(argv[1], "-d") == 0) 
	{
		syslog(LOG_INFO,"Running as daemon");
		daemon_flag = true;
	}
	else
		syslog(LOG_INFO,"Not running as daemon");

	signal(SIGINT, signal_handler);  // Setup signals handlers
	signal(SIGTERM, signal_handler);


	// Struct for sockets
	struct addrinfo hints, *servinfo;
	struct sockaddr_storage their_addr; // connector's/clients address information
	socklen_t sin_size = sizeof(struct sockaddr_storage);

	int yes=1;
	char s[INET6_ADDRSTRLEN];
	char buf[MAX_BUFFER_SIZE];  // Buffer to manipulate data	
	int bytes_received = 0;  


	memset(&hints, 0, sizeof hints);
	hints.ai_family = AF_INET;  // For IPV4 
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_flags = AI_PASSIVE;

	
	file_fd = open(DATA_FILE_PATH, O_RDWR | O_CREAT | O_APPEND, 0644);  

	if(file_fd == ERROR)
	{
		perror("File open");
		syslog(LOG_ERR, "Failed to open data file");
		closelog();
		return ERROR;
	}

	while(1)
	{
		// Get socket address information
		int status = getaddrinfo(NULL, PORT, &hints, &servinfo);

		if (status != 0)	
		{
			fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(status));
			syslog(LOG_ERR,"getaddrinfo failed");
			error_flag = true;
			break;
		}

		if(servinfo == NULL)
		{
			perror("Malloc for getaddrinfo failed");
			syslog(LOG_ERR, " Malloc for getaddrinfo failed");
			error_flag = true;
			break;
		}

		// Create endpoint for communication using socket for server
		server_fd = socket(servinfo->ai_family, servinfo->ai_socktype, servinfo->ai_protocol);

		if(server_fd == ERROR)
		{
			perror("Server socket creation failed");
			syslog(LOG_ERR,"Server socket creation failed");
			error_flag = true;
			break;
		}

		if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int)) == ERROR)
		{
			perror("setsockopt failure");
			syslog(LOG_ERR, "setsockopt failed");
			error_flag = true;
			break;
		}

		// Binding address to server socket
		if (bind(server_fd, servinfo->ai_addr, servinfo->ai_addrlen) == ERROR) 
		{
			perror("server:bind");
			syslog(LOG_ERR, "Binding failure at server");
			error_flag = true;
			break;
		}

		freeaddrinfo(servinfo);  // Free addr structure

		// Check if application is run in daemon mode
		if (daemon_flag == true)
			daemon_mode();

		if(listen(server_fd, BACKLOG) == ERROR)
		{
			perror("listen");
			syslog(LOG_ERR, "listen failed");
			error_flag = true;
			break;
		}

		while(1)
		{
			// Accept client connections
			client_fd = accept(server_fd, (struct sockaddr *)&their_addr, &sin_size);

			if(client_fd == ERROR)
			{
				perror("accept");
				syslog(LOG_ERR,"Accept request failed");
				error_flag = true;
				break;
			}

			inet_ntop(their_addr.ss_family,get_in_addr((struct sockaddr *)&their_addr),
					s, sizeof s);
			printf("server: got connection from %s\n", s);

			syslog(LOG_INFO,"Accepted connection from %s", s);

			memset(buf, '\0', MAX_BUFFER_SIZE); // Clear the buffer
			
			while(1)
			{
				bytes_received = recv(client_fd, buf, MAX_BUFFER_SIZE-1, 0);

				if (bytes_received == ERROR)
				{
					perror("recv");
					syslog(LOG_ERR, " Receiving from client failed");
					error_flag = true;
					break;
				}

				if(write(file_fd, buf, bytes_received) == ERROR)
				{
					perror("File write error");
					syslog(LOG_ERR,"File write error");
					error_flag = true;
					break;
				}

				// Check if packet end has reached
				if(buf[bytes_received-1] == '\n')
					break;
			}

			// Check if error occured er 
			if(error_flag == true)
				break;

			if(lseek(file_fd , 0, SEEK_SET) == ERROR)
			{
				perror("lseek");
				syslog(LOG_ERR, "lseek failed");
				error_flag = true;
				break;
			}

			// Read from the file and send it to the client
			int sent_bytes = 0;
			memset(buf, '\0', MAX_BUFFER_SIZE); // Clear the buffer
			while(1)
			{
				int bytes_read = read(file_fd, buf, MAX_BUFFER_SIZE);

				if(bytes_read == ERROR)
				{
					perror("File read");
					syslog(LOG_ERR, "Failed to read file");
					error_flag = true;
					break;
				}


				sent_bytes = send(client_fd, buf, bytes_read, 0);

				if(sent_bytes == ERROR)
				{
					perror("Send to client failed");
					syslog(LOG_ERR, " Send to client failed");
					error_flag = true;
					break;
				}

				// Check for end of packet
				if(buf[sent_bytes-1] == '\n')
					break;
			}

			// Check if any error occured while sending packets to client
			if (error_flag == true)
				break;

			close(client_fd);
			syslog(LOG_INFO, "Closed connection from %s",s);
		}
		if(error_flag == true)
			break;
	}

	cleanup();

	if(error_flag == true)
		return (ERROR);
	else
		return 0;
}

