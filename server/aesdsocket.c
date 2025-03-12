/*
*   Author: Hardik Minocha
*   Date Created: 02/15/2025
*   External References: 
*   https://linux.die.net/man/ 
*   https://stackoverflow.com/questions/59959206/how-can-i-convert-this-string-into-a-human-readable-ip-address-in-php
*   
*/


#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <syslog.h>
#include <signal.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <pthread.h>  // For threading support
#include <time.h>     // For timestamp functionality


// Optional: Enable debug logging
#ifdef DEBUG
#define DEBUG_LOG(msg,...) printf("socket: " msg "\n" , ##__VA_ARGS__)
#define ERROR_LOG(msg,...) printf("socket ERROR: " msg "\n" , ##__VA_ARGS__)
#else
#define DEBUG_LOG(msg,...)
#define ERROR_LOG(msg,...)
#endif

#define PORT "9000" 
#define BUFFER_SIZE 1024
#define FILE_PATH "/var/tmp/aesdsocketdata"
#define TIMESTAMP_INTERVAL 10  // seconds

volatile sig_atomic_t keep_running = 1;

// Thread connection structure
struct thread_data {
    int client_fd;
    pthread_t thread_id;
    struct thread_data *next;
    char client_ip[INET_ADDRSTRLEN];
};

// Global variables
pthread_mutex_t file_mutex;
struct thread_data *thread_list_head;
pthread_mutex_t list_mutex;
timer_t timestamp_timer;

// User Function Declarations

void handle_signal(int signal_number);
void setup_signal_handling(void);
void make_daemon(void);
int init_server_socket(void);
void* spawn_new_client_connection(void* thread_arg);
void timestamp_handler(union sigval sv);
void setup_timestamp_timer();
void add_thread_to_list(struct thread_data *node);
void cleanup_threads();


int main(int argc, char *argv[]) {
    int server_fd, client_fd;
    char recv_buffer[BUFFER_SIZE];
    FILE *data_file;
    int create_daemon_flag = 0;

    // Parse command line arguments
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-d") == 0) {
            create_daemon_flag = 1;
        }
    }

    // Initialize logging
    openlog("aesdsocket", LOG_PID | LOG_CONS, LOG_USER);
    syslog(LOG_INFO, "Starting aesdsocket server");
    DEBUG_LOG("Server initialization started");

    // Setup signal handlers
    setup_signal_handling();

    // Initialize the server socket
    server_fd = init_server_socket();
    if (server_fd == -1){
        ERROR_LOG("Socket Creation did not return a server_fd...Exiting.");
        return -1;
    }

    // Once the socket is initialised and listening, we should make this a 
    // background process if requested by "-d" argument
    // Daemonize if requested
    if (create_daemon_flag) {
        make_daemon();
    }

    setup_timestamp_timer();

    while (keep_running) {
        struct sockaddr_storage client_addr;
        socklen_t client_len = sizeof(client_addr);

        // Accept connection
        if ((client_fd = accept(server_fd, (struct sockaddr *)&client_addr, &client_len)) < 0) {
            if (keep_running) {
                ERROR_LOG("Socket accept failed");
            }
            continue;
        }

        struct thread_data *thread_node = malloc(sizeof(struct thread_data));
        thread_node->client_fd = client_fd;
        inet_ntop(AF_INET, &((struct sockaddr_in*)&client_addr)->sin_addr, 
                 thread_node->client_ip, INET_ADDRSTRLEN);

        if (pthread_create(&thread_node->thread_id, NULL, 
                         spawn_new_client_connection, thread_node) != 0) {
            ERROR_LOG("Failed to create thread");
            free(thread_node);
            close(client_fd);
            continue;
        }

        add_thread_to_list(thread_node);
        syslog(LOG_INFO, "Accepted connection from %s", thread_node->client_ip);
    }



    // Cleanup
    cleanup_threads();
    timer_delete(timestamp_timer);
    close(server_fd);
    if (remove(FILE_PATH) == 0) {
        syslog(LOG_INFO, "Deleted file %s", FILE_PATH);
    } else {
        ERROR_LOG("Failed to delete file %s", FILE_PATH);
    }

    // Close syslog
    closelog();
    return 0;
}





// User Function Definitions

/*
* Server Socket Initialization:
* Binds a socket file descriptor to an address and starts listening
*/
int init_server_socket(void){

    int server_fd;
    struct addrinfo search_hints, *addr_result;

        // Initialize socket parameters
    memset(&search_hints, 0, sizeof(search_hints));
    search_hints.ai_family = AF_INET;        // IPv4
    search_hints.ai_socktype = SOCK_STREAM;  // TCP
    search_hints.ai_flags = AI_PASSIVE;      // Get a random IP address

    // Get available address info
    if (getaddrinfo(NULL, PORT, &search_hints, &addr_result) != 0) {
        ERROR_LOG("Failed to get address info");
        return -1;
    }

    // There should be just one entry with port 9000, and other specified search parameters,
    // so we're not looking into the whole list.
    // We check if the return addresses list is NULL.
    if (addr_result == NULL) {
        ERROR_LOG("getaddrinfo failed to return available addresses...");
        freeaddrinfo(addr_result); // Free the linked list
        return -1;
    }    

    // Create socket
    if ((server_fd = socket(addr_result->ai_family, addr_result->ai_socktype, addr_result->ai_protocol)) == -1) {
        ERROR_LOG("Socket creation failed");
        return -1;
    }   

    // Bind socket
    if (bind(server_fd, addr_result->ai_addr, addr_result->ai_addrlen) != 0) {
        ERROR_LOG("Socket binding failed");
        return -1;
    }

    freeaddrinfo(addr_result); // Free the linked list


    // Listen for connections
    if (listen(server_fd, 128) != 0) {
        ERROR_LOG("Socket could not listen to port %s", PORT);
        close(server_fd);
        return -1;
    }

    return server_fd;
}

/*
* Signal handler for graceful termination
*/
void handle_signal(int signal_number) {
    if(signal_number == SIGINT || signal_number == SIGTERM) {
        keep_running = 0;
        cleanup_threads();
        syslog(LOG_INFO, "Caught signal, initiating graceful termination");
    }
}

/*
* Register signal handlers for SIGINT and SIGTERM
*/
void setup_signal_handling(void) {
    struct sigaction new_action;
    memset(&new_action, 0, sizeof(struct sigaction));
    new_action.sa_handler = handle_signal;
    
    if (sigaction(SIGINT, &new_action, NULL) != 0) {
        ERROR_LOG("Failed to setup SIGINT handler");
        exit(1);
    }
    if (sigaction(SIGTERM, &new_action, NULL) != 0) {
        ERROR_LOG("Failed to setup SIGTERM handler");
        exit(1);
    }
}

/*
* Create a daemon process
* Fork and terminate parent process.
*/
void make_daemon(void) {
    pid_t process_id = fork();
    if (process_id < 0) {
        ERROR_LOG("Fork failed");
        exit(EXIT_FAILURE);
    }
    if (process_id > 0) {
        // This is still parent process, terminate it
        DEBUG_LOG("Parent process terminated");
        exit(EXIT_SUCCESS);
    }
}

// Function to handle client connection in a thread
void* spawn_new_client_connection(void* thread_arg) {
    struct thread_data *data = (struct thread_data*)thread_arg;
    char recv_buffer[BUFFER_SIZE];
    FILE *data_file;

    while (keep_running) {
        ssize_t bytes_received = recv(data->client_fd, recv_buffer, sizeof(recv_buffer) - 1, 0);
        if (bytes_received <= 0) break;

        recv_buffer[bytes_received] = '\0';

        // Lock mutex before file operations
        pthread_mutex_lock(&file_mutex);
        data_file = fopen(FILE_PATH, "a+");
        if (data_file) {
            fputs(recv_buffer, data_file);
            fclose(data_file);
            
            // Send complete file back to client
            data_file = fopen(FILE_PATH, "r");
            while (fgets(recv_buffer, sizeof(recv_buffer), data_file)) {
                send(data->client_fd, recv_buffer, strlen(recv_buffer), 0);
            }
            fclose(data_file);
        }
        pthread_mutex_unlock(&file_mutex);
    }

    close(data->client_fd);
    syslog(LOG_INFO, "Closed connection from %s", data->client_ip);
    return NULL;
}

// Timestamp handler
void timestamp_handler(union sigval sv) {
    time_t now;
    struct tm *time_info;
    char timestamp[100];
    
    time(&now);
    time_info = localtime(&now);
    strftime(timestamp, sizeof(timestamp), "timestamp:%a, %d %b %Y %H:%M:%S %z\n", time_info);

    pthread_mutex_lock(&file_mutex);
    FILE *file = fopen(FILE_PATH, "a");
    if (file) {
        fputs(timestamp, file);
        fclose(file);
    }
    pthread_mutex_unlock(&file_mutex);
}

// Setup timer for timestamps
void setup_timestamp_timer() {
    struct sigevent sev;
    struct itimerspec its;

    sev.sigev_notify = SIGEV_THREAD;
    sev.sigev_notify_function = timestamp_handler;
    sev.sigev_notify_attributes = NULL;
    sev.sigev_value.sival_ptr = NULL;

    timer_create(CLOCK_REALTIME, &sev, &timestamp_timer);

    its.it_value.tv_sec = TIMESTAMP_INTERVAL;
    its.it_value.tv_nsec = 0;
    its.it_interval.tv_sec = TIMESTAMP_INTERVAL;
    its.it_interval.tv_nsec = 0;

    timer_settime(timestamp_timer, 0, &its, NULL);
}

// Add node to thread list
void add_thread_to_list(struct thread_data *node) {
    pthread_mutex_lock(&list_mutex);
    node->next = thread_list_head;
    thread_list_head = node;
    pthread_mutex_unlock(&list_mutex);
}

// Clean up threads
void cleanup_threads() {
    pthread_mutex_lock(&list_mutex);
    struct thread_data *current = thread_list_head;
    struct thread_data *next;

    while (current != NULL) {
        next = current->next;
        pthread_join(current->thread_id, NULL);
        free(current);
        current = next;
    }
    thread_list_head = NULL;
    pthread_mutex_unlock(&list_mutex);
}
