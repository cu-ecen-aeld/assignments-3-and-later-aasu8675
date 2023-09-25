/**
 * Compiled on Virtual Box 
 * Compiler Flags Used: -Wall & -Werror
 * @file    systemcalls.c
 * @brief   Assignment 4 of course ECEN-5713 (AESD)
 * @author  Aamir Suhail Burhan
 * @version 1.0
 * @Submission Date: 24th September 2023
 *
 * @description  The threading.c application consists of functions which are required to create
 * a pthread
 *
 */


/***************************************************HEADER FILES***********************************/
#include "threading.h"
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>


/***************************************************DEFINES****************************************/
#define SUCCESS (0)

// Optional: use these functions to add debug or error prints to your application
#define DEBUG_LOG(msg,...) printf("threading: " msg "\n" , ##__VA_ARGS__)
#define ERROR_LOG(msg,...) printf("threading ERROR: " msg "\n" , ##__VA_ARGS__)

void* threadfunc(void* thread_param)
{

    // TODO: wait, obtain mutex, wait, release mutex as described by thread_data structure
    // hint: use a cast like the one below to obtain thread arguments from your parameter
    //struct thread_data* thread_func_args = (struct thread_data *) thread_param;
    struct thread_data* thread_func_args = (struct thread_data *)thread_param;

    thread_func_args->thread_complete_success = false;  // Default status of thread completion
    
    // Sleep time till mutex is obtained
    usleep(thread_func_args->wait_to_obtain_ms * 1000);
    
    // Check if mutex lock was performed successfully
    if(pthread_mutex_lock(thread_func_args->mutex) != SUCCESS)
    {
	ERROR_LOG("Mutex locking Unsuccessful");
    }	
    else
    {
	usleep(thread_func_args->wait_to_release_ms * 1000);
	DEBUG_LOG("Waiting to release mutex lock");

	int release_status = pthread_mutex_unlock(thread_func_args->mutex);  // Releasing the mutex
	if (release_status == SUCCESS)
	{
       	    DEBUG_LOG("Mutex Successfully Unlocked");
	    thread_func_args->thread_complete_success = true;
	}
	else
	{
	    ERROR_LOG("Mutex Unlocking Failed");
	}
    }

    return thread_param;
}


bool start_thread_obtaining_mutex(pthread_t *thread, pthread_mutex_t *mutex,int wait_to_obtain_ms, int wait_to_release_ms)
{
    /**
     * TODO: allocate memory for thread_data, setup mutex and wait arguments, pass thread_data to created thread
     * using threadfunc() as entry point.
     *
     * return true if successful.
     *
     * See implementation details in threading.h file comment block
     */

    // Allocate memory for thread_data
    struct thread_data* data = (struct thread_data*)malloc(sizeof(struct thread_data));
    
    if (data == NULL) 
    {
        ERROR_LOG("Failed to allocate memory for thread data");
        return false;
    }

    DEBUG_LOG("Memory Allocation for thread was successful");
    
    // Initialize thread data with parameters
    data->mutex = mutex;
    data->wait_to_obtain_ms = wait_to_obtain_ms;
    data->wait_to_release_ms = wait_to_release_ms;
    data->thread_complete_success = false;  // Initial completion status of thread


    // Thread creation
    int pthread_create_status;
    pthread_create_status = pthread_create(thread, NULL, threadfunc, data);

    if (pthread_create_status == SUCCESS)
    {
        DEBUG_LOG("Thread creation Successful");
	return true;
    }
    else
    {
	ERROR_LOG("Failed to create thread");
	free(data);  // Free malloced memory
    }

    return false;
}

