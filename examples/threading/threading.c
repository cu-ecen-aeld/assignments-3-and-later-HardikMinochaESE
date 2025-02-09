/*
*
*    Author: Hardik Minocha
*    Date Created: Feb 8 2025
*
*/

#include "threading.h"
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>

// #define debug

#ifdef debug
// Optional: use these functions to add debug or error prints to your application
//#define DEBUG_LOG(msg,...)
#define DEBUG_LOG(msg,...) printf("threading: " msg "\n" , ##__VA_ARGS__)
#define ERROR_LOG(msg,...) printf("threading ERROR: " msg "\n" , ##__VA_ARGS__)

#else

#define DEBUG_LOG(msg,...)
#define ERROR_LOG(msg,...)

#endif

void* threadfunc(void* thread_param)
{

    // TODO: wait, obtain mutex, wait, release mutex as described by thread_data structure
    // hint: use a cast like the one below to obtain thread arguments from your parameter
    //struct thread_data* thread_func_args = (struct thread_data *) thread_param;

    DEBUG_LOG("Entered the thread!");
    struct thread_data* thread_func_args = (struct thread_data *) thread_param; 
    int rc = -1;
    
    
    // Sleep for "wait_to_obtain_ms" milliseconds
    DEBUG_LOG("Sleeping for %d milliseconds before locking mutex",thread_func_args->wait_to_obtain_ms);
    usleep((thread_func_args->wait_to_obtain_ms)*1000);
    
    // Obtain pthread_mutex_t *mutex
    DEBUG_LOG("Trying to Lock Mutex");
    rc = pthread_mutex_lock(thread_func_args->mutex);
    if(rc != 0){
    	ERROR_LOG("Failed to lock mutex...  Returning to parent function with failure! rc:%d",rc);
    	thread_func_args->thread_complete_success = false;
    	return thread_param;
    }
    
    // Hold the mutex for "wait_to_release_ms" milliseconds
    DEBUG_LOG("Sleeping for %d milliseconds before unlocking mutex",thread_func_args->wait_to_obtain_ms);
    usleep((thread_func_args->wait_to_release_ms)*1000);
    
    
    // Release pthread_mutex_t *mutex
    DEBUG_LOG("Trying to Unlock Mutex");
    rc = pthread_mutex_unlock(thread_func_args->mutex);
    if(rc != 0){
    	ERROR_LOG("Failed to unlock mutex... May lead to unbounded blockage!! rc:%d",rc);
    	thread_func_args->thread_complete_success = false;
    	return thread_param;
    }
    
    
    // Set the complete flag to true
    thread_func_args->thread_complete_success = true;
    
    DEBUG_LOG("Ending Thread!");
    
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
     
     // initialize a return code checking variable
     int rc = -1;
     pthread_t thread_ID;
     
     // Dynamically allocate struct thread_data instance
     struct thread_data *thread_param_struct = (struct thread_data *)malloc(sizeof(struct thread_data *)); 
     
     // If mutex was initialized, save it to the allocated struct
     thread_param_struct->mutex = mutex;
     // Load the wait arguments into the allocated struct
     thread_param_struct->wait_to_obtain_ms = wait_to_obtain_ms;
     thread_param_struct->wait_to_release_ms = wait_to_release_ms;
     
     // DEBUG_LOG("Wait time before obtaining is %d", thread_param_struct->wait_to_obtain_ms);
     // DEBUG_LOG("Wait time after obtaining is %d", thread_param_struct->wait_to_release_ms);
     
     
     // Use the allocated struct to spawn a thread executing the threadfunc() function
     // If thread created succesfully, save the pthread_create() thread ID in *thread
     rc = -1;
     DEBUG_LOG("Creating Thread...");
     rc = pthread_create(&thread_ID, NULL, threadfunc, thread_param_struct);
     if(rc != 0){
    	 ERROR_LOG("Failed to create thread!...  rc:%d\n",rc);
    	 free(thread_param_struct);
    	 return false;
     }
     else{
     	*thread = thread_ID;
     }
     DEBUG_LOG("Created the thread and moved on...");
     
     
     // Return True if thread could be started, False if a failure ocurred.
     
     return (thread_ID != 0)?true:false;
}

