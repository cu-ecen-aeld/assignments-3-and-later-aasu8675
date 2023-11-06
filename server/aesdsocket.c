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
#include <sys/stat.h>
#include "queue.h"
#include "../aesd-char-driver/aesd_ioctl.h"

/*****************************************DEFINES***********************************/
#define PORT "9000"
#define ERROR (-1)
#define SUCCESS (0)
#define BACKLOG (10)  // Number of pending connection queue will hold
#define MAX_BUFFER_SIZE 1024
#define TIME_STAMP_INTERVAL_IN_SECS (10)


// Used as a build switch for device driver
#define USE_AESD_CHAR_DEVICE

// Using buildswitch for character device driver
#ifdef USE_AESD_CHAR_DEVICE
#define DATA_PATH "/dev/aesdchar"
#else
#define DATA_PATH "/var/tmp/aesdsocketdata"
#endif


int server_fd, client_fd;  // File descriptors for server and client
int file_fd;  // Files descriptor for the data file
bool daemon_flag = false;
volatile sig_atomic_t exit_flag = 0;

/******************Structure defintion of linked list based thread******************/
typedef struct client_node 
{
	pthread_t thread_id;
	int connection_fd;
	bool thread_completion_status;
	pthread_mutex_t *thread_mutex;
	char *ioctl_string;
	SLIST_ENTRY(client_node) next_node;  // Pointer to next elemenet
} client_node_t;

#ifndef USE_AESD_CHAR_DEVICE
// Struct for timestamp thread
typedef struct timestamp
{
	pthread_t thread_id;
	pthread_mutex_t *thread_mutex;
} timestamp_t;


timestamp_t *time_node = NULL;
#endif


/* Description: This function closes all file descriptors and deletes the created 
 * file.
 */
void cleanup(void)
{
	syslog(LOG_DEBUG, "Cleanup start");

	if(close(server_fd) == ERROR)
	{
		perror("Server fd");
		syslog(LOG_ERR,"Server fd close");
	}

	// Error handling for closing fd
	if(close(file_fd) == ERROR)
	{
		perror("File close");
		syslog(LOG_ERR, "Error in closing file");
	}

	// Do not remove the driver
#ifndef USE_AESD_CHAR_DEVICE
	// Error handling for deleting file
	if(remove(DATA_PATH) == ERROR)
	{
		perror("File removal");
		syslog(LOG_ERR, " File removal failed");
	}
#endif

	syslog(LOG_DEBUG, "Cleanup End");
	closelog();
}


/* Description: This function provides support to run this application
 * as a daemon
 */
int daemon_mode(void)
{
	pid_t process_id = fork();  // Forking the parent
	if (process_id == ERROR)
	{
		perror("Fork failed");
		syslog(LOG_ERR, "Fork failed");
		return ERROR;
	}

	// Valid process id
	if(process_id)
	{
		syslog(LOG_INFO,"Exiting parent process");
		exit(0);
	}

	umask(0);  // Unmask the file mode

	// Start a new session
	if(setsid() == ERROR)
	{
		perror("setsid");
		syslog(LOG_ERR, "Error in creating daemon session");
		return ERROR;
	}

	// Change directory to root
	if(chdir("/") == ERROR)
	{
		perror("Chdir failure");
		syslog(LOG_ERR, "chdir failed");
		return ERROR;
	}

	// Close standard file descriptors
	close(STDIN_FILENO);
	close(STDOUT_FILENO);
	close(STDERR_FILENO);

	// Redirecting output to standard files
	int fd = open("/dev/null", O_RDWR);
	if (fd == ERROR)
	{
		syslog(LOG_ERR, "Redirecting open");
		perror("Redirecting open");
		return ERROR;
	} 

	if (dup2(fd, STDIN_FILENO) == ERROR)
	{
		syslog(LOG_ERR, "Redirecting stdin");
		perror("Redirecting stdin");
		return ERROR;
	}


	if (dup2(fd, STDOUT_FILENO) == ERROR)
	{
		syslog(LOG_ERR, "Redirecting stdout");
		perror("Redirecting stdout");
		return ERROR;
	}


	if (dup2(fd, STDERR_FILENO) == ERROR)
	{
		syslog(LOG_ERR, "Redirecting stderr");
		perror("Redirecting stderr");
		return ERROR;
	}

	close(fd);

	return 0;
}


/* Description: This function handles the signals SIGINT and SIGTERM and gracefully 
 * exits the program on receiving them.
 */
void signal_handler(int signo)
{
	syslog(LOG_DEBUG, "Entered signal handler");
	if (signo == SIGINT || signo == SIGTERM)
	{
		syslog(LOG_INFO,"Caught signal,exiting");

		exit_flag = 1;

		shutdown(server_fd, SHUT_RDWR);

		close(server_fd);

#ifndef USE_AESD_CHAR_DEVICE
		pthread_cancel(time_node -> thread_id);
#endif
	}
	syslog(LOG_DEBUG, "Exiting signal handler");
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

#ifndef USE_AESD_CHAR_DEVICE
// Function to append a timestamp to the file
void  *timestamp_appender(void *thread_node) 
{
	timestamp_t* timestamp_data = (timestamp_t*)thread_node;
	int ret;

	struct timespec ts;
	struct tm *local_time_info;
	time_t current_time;
	char timestamp[MAX_BUFFER_SIZE];

	while(1)
	{
		ret = clock_gettime(CLOCK_MONOTONIC, &ts);

		if(ret == ERROR)
		{
			perror("Clock gettime");
			syslog(LOG_ERR, "Clock gettime");
			return NULL;
		}

		ts.tv_sec += TIME_STAMP_INTERVAL_IN_SECS;  // Adding 10 seconds interval

		ret = clock_nanosleep(CLOCK_MONOTONIC, TIMER_ABSTIME, &ts, NULL);

		if(ret == ERROR)
		{
			perror("Clock nanosleep");
			syslog(LOG_ERR, "Clock nanosleep");
			return NULL;
		}

		time(&current_time);
		local_time_info = localtime(&current_time);

		// Format the timestamp in RFC 2822 compliant strftime format
		strftime(timestamp, sizeof(timestamp), "timestamp: %Y/%m/%d %H:%M:%S\n", local_time_info);

		if(pthread_mutex_lock(timestamp_data -> thread_mutex) != SUCCESS)
		{
			perror("Mutex Lock");
			syslog(LOG_ERR, "Mutex Lock");
			return NULL;
		}

		if(write(file_fd, timestamp, strlen(timestamp)) == ERROR)
		{
			perror("Timestamp file write");
			syslog(LOG_ERR, "Timestamp file write");
			return NULL;
		}

		if(pthread_mutex_unlock(timestamp_data -> thread_mutex) != SUCCESS)
		{
			perror("Mutex Unlock");
			syslog(LOG_ERR, "Mutex Unlock");
			return NULL;
		}
	}
}
#endif


/* Description: This function handles the client connections in a thread
*/
void *client_handler(void *client_thread)
{
	if (client_thread == NULL) 
	{
		perror("Client thread is NULL");
		syslog(LOG_ERR, "Client thread is NULL");
		return NULL;
	}

	char buf[MAX_BUFFER_SIZE];
	int bytes_received = 0;
	int file_fd;
	bool newline_status = false;

	client_node_t *node = (client_node_t *)client_thread;
	node->thread_completion_status = false;

	node->ioctl_string = "AESDCHAR_IOCSEEKTO:"; // Ioctl setup
	memset(buf, '\0', MAX_BUFFER_SIZE);           // Clear the buffer

	file_fd = open(DATA_PATH, O_RDWR, 0644);
	if (file_fd == ERROR) 
	{
		perror("File open error");
		syslog(LOG_ERR, "File Open error");
	} 
	else 
	{
		syslog(LOG_INFO, "Opened aesdchar driver successfully");
	}

	while (1) 
	{
		bytes_received = recv(node->connection_fd, buf, MAX_BUFFER_SIZE, 0);


		if (bytes_received == ERROR) 
		{
			perror("recv from handler");
			syslog(LOG_ERR, "Receiving from client failed");
			goto exit;
		}

		// Lock the mutex before writing to the file
		if (pthread_mutex_lock(node->thread_mutex) != SUCCESS)
		{
			perror("Mutex lock failure");
			syslog(LOG_ERR, "Mutex lock failure");
			goto exit;
		}

		// Check if packet end has reached
		if ((memchr(buf, '\n', bytes_received)) != NULL)
			newline_status = true;

		if(newline_status == true)
		{
			// Check if the received string starts with "AESDCHAR_IOCSEEKTO:"
			if (strncmp(buf, node->ioctl_string, strlen(node->ioctl_string)) == 0) 
			{
				// Extract X and Y from the string
				struct aesd_seekto seekto;
				sscanf(buf, "AESDCHAR_IOCSEEKTO:%d,%d", &seekto.write_cmd, &seekto.write_cmd_offset); 

				if (ioctl(file_fd, AESDCHAR_IOCSEEKTO, &seekto)) 
				{
					perror("ioctl write command");
					syslog(LOG_ERR, "ioctl write command");
					goto exit;
				}
			} 
			else 
			{
				if (write(file_fd, buf, bytes_received) == ERROR) 
				{
					perror("File write error");
					syslog(LOG_ERR, "File write error");
					goto exit;
				}

				// Update pos
				//node->file_pos += bytes_received;
			}

			if (pthread_mutex_unlock(node->thread_mutex) != SUCCESS)
			{
				perror("Mutex unlock failure");
				syslog(LOG_ERR, "Mutex unlock failure");
				goto exit;
			}
		}
	}

	// Read from the file and send it to the client
	int sent_bytes = 0;
	memset(buf, '\0', MAX_BUFFER_SIZE); // Clear the buffer

#ifndef USE_AESD_CHAR_DEVICE
	if (lseek(file_fd, 0, SEEK_SET) == ERROR) 
	{
		perror("lseek");
		syslog(LOG_ERR, "lseek failed");
		goto exit;
	}
#endif

//	int temp_pos = node->file_pos;
	while (1) 
	{
		if (pthread_mutex_lock(node->thread_mutex) != SUCCESS)
		{
			perror("Mutex lock failure");
			syslog(LOG_ERR, "Mutex lock failure");
			goto exit;
		}

		int bytes_read = read(file_fd, buf, MAX_BUFFER_SIZE);

		if (pthread_mutex_unlock(node->thread_mutex) != SUCCESS)
		{
			perror("Mutex unlock failure");
			syslog(LOG_ERR, "Mutex unlock failure");
			goto exit;
		}

		if (bytes_read == ERROR) 
		{
			perror("File read");
			syslog(LOG_ERR, "Failed to read file");
			goto exit;
		}

		sent_bytes = send(node->connection_fd, buf, bytes_read, 0);

		if (sent_bytes == ERROR) 
		{
			perror("Send to client failed");
			syslog(LOG_ERR, "Send to client failed");
			goto exit;
		}

		//temp_pos -= sent_bytes;

		// If there are no more bytes to read, exit the while loop
		if (bytes_read == 0)
			break;
	}

	node->thread_completion_status = true;

exit:
	close(node->connection_fd);
	close(file_fd);
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


	// Get socket address information
	int status = getaddrinfo(NULL, PORT, &hints, &servinfo);

	if (status != 0)	
	{
		fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(status));
		syslog(LOG_ERR,"getaddrinfo failed");
		if(servinfo != NULL)
			freeaddrinfo(servinfo);
		return ERROR;
	}

	if(servinfo == NULL)
	{
		perror("Malloc for getaddrinfo failed");
		syslog(LOG_ERR, " Malloc for getaddrinfo failed");
		return ERROR;
	}

	// Create endpoint for communication using socket for server
	server_fd = socket(servinfo->ai_family, servinfo->ai_socktype, servinfo->ai_protocol);

	if(server_fd == ERROR)
	{
		syslog(LOG_ERR, "listen failed");
		return ERROR;
	}

	if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int)) == ERROR)
	{
		perror("setsockopt failure");
		syslog(LOG_ERR, "setsockopt failed");
		return ERROR;
	}

	// Binding address to server socket
	if (bind(server_fd, servinfo->ai_addr, servinfo->ai_addrlen) == ERROR)
	{
		perror("server:bind");
		syslog(LOG_ERR, "Binding failure at server");
		if(servinfo != NULL)
			freeaddrinfo(servinfo);
		return ERROR;
	}

	freeaddrinfo(servinfo);  // Free addr structure

	int daemon_status = 0;

	// Check if application is run in daemon mode
	if (daemon_flag == true)
		daemon_status = daemon_mode();

	// Check if daemon mode caused any failure
	if (daemon_status == ERROR)
	{
		return ERROR;
	}

	if(listen(server_fd, BACKLOG) == ERROR)
	{
		perror("listen");
		syslog(LOG_ERR, "listen failed");
		return ERROR;
	}

	syslog(LOG_DEBUG, "Listening for connections");

#ifndef USE_AESD_CHAR_DEVICE
	// Malloc for timer thread
	time_node = (timestamp_t*)malloc(sizeof(timestamp_t));

	if(time_node == NULL)
	{
		perror("Malloc fail on timestamp thread");
		syslog(LOG_ERR, "Malloc fail on timestamp thread");
		return ERROR;
	}


	time_node -> thread_mutex = &thread_mutex;

	if(pthread_create(&time_node -> thread_id, NULL, timestamp_appender, time_node) != SUCCESS)
	{
		perror("pthread_create() for timer thread");
		syslog(LOG_ERR, "pthread_create() for timer thread");
		free(time_node);
		time_node = NULL;
		return ERROR;
	} 
#endif

	while(!exit_flag)
	{
		// Accept client connections
		client_fd = accept(server_fd, (struct sockaddr *)&their_addr, &sin_size);

		if(client_fd == ERROR)
		{
			perror("accept");
			syslog(LOG_ERR,"Accept request failed");
			if(exit_flag == 0)
				return ERROR;
			else
				break;
		}

		syslog(LOG_DEBUG, "Connection accepted");

		inet_ntop(their_addr.ss_family,get_in_addr((struct sockaddr *)&their_addr),
				s, sizeof s);

		data_node_ptr = (client_node_t *)malloc(sizeof(client_node_t));

		if(data_node_ptr == NULL)
		{
			perror("Malloc for client thread");
			syslog(LOG_ERR, "Malloc for client thread");
			return ERROR;
		}

		data_node_ptr -> connection_fd = client_fd;
		data_node_ptr -> thread_mutex = &thread_mutex;
		data_node_ptr -> thread_completion_status = false;

		if(pthread_create(&(data_node_ptr -> thread_id), NULL, client_handler, data_node_ptr) != SUCCESS)
		{
			perror("pthread_create() for Client connection thread");
			syslog(LOG_ERR, "pthread_create() for Client connection thread");
			free(data_node_ptr);
			data_node_ptr = NULL;
			return ERROR;
		}

		// Insert the node into linked-list
		SLIST_INSERT_HEAD(&head, data_node_ptr, next_node);
		data_node_ptr = NULL;

		// check for thread completion
		SLIST_FOREACH(data_node_ptr, &head, next_node)
		{
			if(data_node_ptr -> thread_completion_status == true)
			{
				int status;
				status = pthread_join(data_node_ptr-> thread_id, NULL);
				if(status == ERROR)
				{
					syslog(LOG_ERR, "Thread join failed");
					return ERROR;
				}

				syslog(LOG_INFO, "Thread join %ld",data_node_ptr -> thread_id);
			}
		}

	}

	syslog(LOG_DEBUG, "Finished while loop");

	// Emptying the Linked-list
	while (!SLIST_EMPTY(&head))
	{
		data_node_ptr = SLIST_FIRST(&head);
		SLIST_REMOVE(&head, data_node_ptr, client_node, next_node);
		free(data_node_ptr);
		data_node_ptr = NULL;
	}

	syslog(LOG_DEBUG, "Finished emptying linked list for client handling threads");

#ifndef USE_AESD_CHAR_DEVICE
	pthread_join(time_node -> thread_id, NULL);  // Timer thread

	syslog(LOG_DEBUG, "Joined timer thread");

	free(time_node);
	time_node = NULL;
	pthread_mutex_destroy(&thread_mutex);
#endif


	syslog(LOG_DEBUG, "Before cleanup in main");
	cleanup();
	syslog(LOG_DEBUG, "After cleanup in main");

	return 0;
}

