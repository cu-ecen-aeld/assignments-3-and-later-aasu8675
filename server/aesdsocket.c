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
 * References: 
 * 1. https://beej.us/guide/bgnet/html/
 * 2. https://raw.githubusercontent.com/freebsd/freebsd/stable/10/sys/sys/queue.h
 * 3. https://learning.oreilly.com/library/view/linux-system-programming/9781449341527/ch11.html#a_portable_way_to_sleep
 *
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
#include <pthread.h>
#include <time.h>
#include "queue.h"

/*****************************************DEFINES***********************************/
#define PORT "9000"
#define ERROR (-1)
#define SUCCESS (0)
#define BACKLOG (10)  // Number of pending connection queue will hold
#define MAX_BUFFER_SIZE 1024
#define DATA_FILE_PATH "/var/tmp/aesdsocketdata"
#define TIME_STAMP_INTERVAL_IN_SECS (10)
int server_fd, client_fd;  // File descriptors for server and client
int file_fd;  // Files descriptor for the data file
bool error_flag = false, daemon_flag = false;


/******************Structure defintion of linked list based thread******************/
typedef struct client_node 
{
	pthread_t thread_id;
	int connection_fd;
	bool thread_completion_status;
	pthread_mutex_t *thread_mutex;
	SLIST_ENTRY(client_node) next_node;  // Pointer to next elemenet
} client_node_t;


// Struct for timestamp thread
typedef struct timestamp
{
	pthread_t thread_id;
	bool thread_completion_status;
	pthread_mutex_t *thread_mutex;
} timestamp_t;

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
		exit(EXIT_SUCCESS);
	}
}


// get sockaddr, IPv4 or IPv6:
void *get_in_addr(struct sockaddr *sa)
{
	if (sa->sa_family == AF_INET) 
	{
		return &(((struct sockaddr_in*)sa)->sin_addr);
	}
	return &(((struct sockaddr_in6*)sa)->sin6_addr);
}


// Function to append a timestamp to the file
void  *timestamp_appender(void *thread_node) 
{
	timestamp_t* timestamp_data = (timestamp_t*)thread_node;
	timestamp_data -> thread_completion_status = false;
	int ret;
	bool timestamp_error_status = false;

	struct timespec ts;
	struct tm *local_time_info;
	time_t current_time;
	char timestamp[MAX_BUFFER_SIZE];

	while (1) 
	{
		if(error_flag == true)
			break;

		ret = clock_gettime(CLOCK_MONOTONIC, &ts);

		if(ret == ERROR)
		{
			perror("Clock gettime");
			syslog(LOG_ERR, "Clock gettime");
			timestamp_error_status = true;
			break;
		}

		ts.tv_sec += TIME_STAMP_INTERVAL_IN_SECS;  // Adding 10 seconds interval

		ret = clock_nanosleep(CLOCK_MONOTONIC, TIMER_ABSTIME, &ts, NULL);

		if(ret == ERROR)
		{
			perror("Clock nanosleep");
			syslog(LOG_ERR, "Clock nanosleep");
			timestamp_error_status = true;
			break;
		}

		time(&current_time);
		local_time_info = localtime(&current_time);

		// Format the timestamp in RFC 2822 compliant strftime format
		strftime(timestamp, sizeof(timestamp), "timestamp: %Y/%m/%d %H:%M:%S\n", local_time_info);

		if(pthread_mutex_lock(timestamp_data -> thread_mutex) != SUCCESS)
		{
			perror("Mutex Lock");
			syslog(LOG_ERR, "Mutex Lock");
			timestamp_error_status = true;
			break;
		}

		if(write(file_fd, timestamp, strlen(timestamp)) == ERROR)
		{
			perror("Timestamp file write");
			syslog(LOG_ERR, "Timestamp file write");
			timestamp_error_status = true;
			break;
		}

		if(pthread_mutex_unlock(timestamp_data -> thread_mutex) != SUCCESS)
		{
			perror("Mutex Unlock");
			syslog(LOG_ERR, "Mutex Unlock");
			timestamp_error_status = true;
			break;
		}

	}

	if(timestamp_error_status == false)
		timestamp_data -> thread_completion_status = true;

	return thread_node;
}


/* Description: This function handles the client connections in a thread
*/
void *client_handler(void *client_thread)
{
	if(client_thread == NULL)
	{
		perror("Client thread is NULL");
		syslog(LOG_ERR, "Client thread si NULL");
		return NULL;
	}

	char buf[MAX_BUFFER_SIZE];
	int bytes_received = 0;
	bool handler_error_status = false;

	client_node_t *node = (client_node_t*) client_thread;
	node -> thread_completion_status = false;

	memset(buf, '\0', MAX_BUFFER_SIZE); // Clear the buffer

	while (1)
	{
		// Loop to receive data from the client
		while (1) 
		{
			bytes_received = recv(node -> connection_fd, buf, MAX_BUFFER_SIZE - 1, 0);

			if (bytes_received == ERROR) 
			{
				perror("recv from handler");
				syslog(LOG_ERR, "Receiving from client failed");
				handler_error_status = true;
				break;
			}

			// Lock the mutex before writing to the file
			if(pthread_mutex_lock(node -> thread_mutex) != SUCCESS)
			{
				perror("Mutex lock failure");
				syslog(LOG_ERR, "Mutex lock failure");
				handler_error_status = true;
				break;
			}

			if (write(file_fd, buf, bytes_received) == ERROR) 
			{
				perror("File write error");
				syslog(LOG_ERR, "File write error");
				handler_error_status = true;
				break;
			}

			// Unlock the mutex after writing to the file
			if(pthread_mutex_unlock(node -> thread_mutex) != SUCCESS)
			{
				perror("Mutex unlock failure");
				syslog(LOG_ERR, "Mutex unlock failure");
				handler_error_status = true;
				break;
			}

			// Check if packet end has reached
			if (buf[bytes_received - 1] == '\n')
				break;
		}

		if(handler_error_status == true)
			break;

		// Read from the file and send it to the client
		int sent_bytes = 0;
		memset(buf, '\0', MAX_BUFFER_SIZE); // Clear the buffer

		if(lseek(file_fd , 0, SEEK_SET) == ERROR)
		{
			perror("lseek");
			syslog(LOG_ERR, "lseek failed");
			handler_error_status = true;
			break;
		}

		while(1)
		{
			int bytes_read = read(file_fd, buf, MAX_BUFFER_SIZE);

			if(bytes_read == ERROR)
			{
				perror("File read");
				syslog(LOG_ERR, "Failed to read file");
				handler_error_status = true;
				break;
			}

			sent_bytes = send(node -> connection_fd, buf, bytes_read, 0);

			if(sent_bytes == ERROR)
			{
				perror("Send to client failed");
				syslog(LOG_ERR, " Send to client failed");
				handler_error_status = true;
				break;
			}

			// Check for end of packet
			if(buf[sent_bytes-1] == '\n')
				break;
		}

		if(handler_error_status == true)
			break;
	}

	//	close(file_fd);
	close(node -> connection_fd);

	if(handler_error_status == false)
		node -> thread_completion_status = true;

	return client_thread;
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
	pthread_mutex_t thread_mutex;

	client_node_t *data_node_ptr = NULL;
	client_node_t *temp = NULL;

	timestamp_t *time_node = NULL;

	if(pthread_mutex_init(&thread_mutex, NULL) != SUCCESS) 
	{
		perror("Mutex Initialization");
		syslog(LOG_ERR, "Mutex Initialization");
		return ERROR;
	}


	int yes=1;
	char s[INET6_ADDRSTRLEN];	


	SLIST_HEAD(client_head, client_node) head;
	SLIST_INIT(&head);  // Initializing the head of the clients to be connected

	memset(&hints, 0, sizeof hints);
	hints.ai_family = AF_INET;  // For IPV4 
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_flags = AI_PASSIVE;


	file_fd = open(DATA_FILE_PATH, O_RDWR | O_APPEND | O_CREAT, 0644);
	if (file_fd == ERROR)
	{
		perror("File open");
		syslog(LOG_ERR, "File Open");
		error_flag = true;
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
			if(servinfo != NULL)
				freeaddrinfo(servinfo);
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
			syslog(LOG_ERR, "listen failed");
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
			if(servinfo != NULL)
				freeaddrinfo(servinfo);
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

		// Malloc for timer thread
		time_node = (timestamp_t*)malloc(sizeof(timestamp_t));

		if(time_node == NULL)
		{
			perror("Malloc fail on timestamp thread");
			syslog(LOG_ERR, "Malloc fail on timestamp thread");
			error_flag = true;
			break;
		}

		time_node -> thread_completion_status = false;
		time_node -> thread_mutex = &thread_mutex;

		if(pthread_create(&time_node -> thread_id, NULL, timestamp_appender, time_node) != SUCCESS)
		{
			perror("pthread_create() for timer thread");
			syslog(LOG_ERR, "pthread_create() for timer thread");
			error_flag = true;
			free(time_node);
			time_node = NULL;
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

			data_node_ptr = (client_node_t *)malloc(sizeof(client_node_t));

			if(data_node_ptr == NULL)
			{
				perror("Malloc for client thread");
				syslog(LOG_ERR, "Malloc for client thread");
				error_flag = true;
				break;
			}

			data_node_ptr -> connection_fd = client_fd;
			data_node_ptr -> thread_mutex = &thread_mutex;
			data_node_ptr -> thread_completion_status = false;

			if(pthread_create(&(data_node_ptr -> thread_id), NULL, client_handler, data_node_ptr) != SUCCESS)
			{
				perror("pthread_create() for Client connection thread");
				syslog(LOG_ERR, "pthread_create() for Client connection thread");
				error_flag = true;
				free(data_node_ptr);
				data_node_ptr = NULL;
				break;
			}

			SLIST_INSERT_HEAD(&head, data_node_ptr, next_node);

			SLIST_FOREACH_SAFE(data_node_ptr, &head, next_node, temp)
			{
				if(data_node_ptr -> thread_completion_status == true)
				{
					syslog(LOG_INFO,"Joining client thread");
					pthread_join(data_node_ptr -> thread_id, NULL);
					SLIST_REMOVE(&head, data_node_ptr, client_node, next_node);
					free(data_node_ptr);
					data_node_ptr = NULL;
				}
			}

			if(error_flag == true)
				break;
		}


		if(error_flag == true)
			break;
	}

	cleanup();
	pthread_mutex_destroy(&thread_mutex);

	// Emptying the Linked-list
	while(!SLIST_EMPTY(&head))
	{
		data_node_ptr = SLIST_FIRST(&head);
		SLIST_REMOVE_HEAD(&head, next_node);
		pthread_join(data_node_ptr -> thread_id, NULL);  // For timer thread
		free(data_node_ptr);
		data_node_ptr = NULL;
	}

	pthread_join(time_node -> thread_id, NULL);  // Timer thread
	free(time_node);
	time_node = NULL;

	if(error_flag == true)
		return 1;
	else
		return 0;
}

