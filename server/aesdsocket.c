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

volatile sig_atomic_t keep_running = 1;


// User Function Declarations

void handle_signal(int signal_number);
void setup_signal_handling(void);
void make_daemon(void);
int init_server_socket(void);


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

        // Log accepted connection
        char *client_ip = inet_ntoa(((struct sockaddr_in*)&client_addr)->sin_addr);
        syslog(LOG_INFO, "Accepted connection from %s", client_ip);

        // Open file for appending, create if doesn't exist.
        data_file = fopen(FILE_PATH, "a+");
        if (!data_file) {
            ERROR_LOG("Failed to openinet_ntoa file %s", FILE_PATH);
            close(client_fd);
            continue;
        }

        // Dynamic buffer for incoming data
        char *data_buffer = NULL;
        size_t data_length = 0;

        // Receive data and handle delimiting and storing it in file.
        while (keep_running) {
            ssize_t bytes_received = recv(client_fd, recv_buffer, sizeof(recv_buffer) - 1, 0);
            if (bytes_received <= 0) {
                break; // Connection closed or error
            }

            recv_buffer[bytes_received] = '\0'; // Null-terminate the received data

            // Append received data to the data buffer
            size_t new_length = data_length + bytes_received;
            // Since we are not sure how much RAM we will have available, use realloc instead of malloc.
            data_buffer = realloc(data_buffer, new_length + 1);
            if (!data_buffer) {
                ERROR_LOG("Memory allocation failed");
                break;
            }
            memcpy(data_buffer + data_length, recv_buffer, bytes_received);
            data_length = new_length;
            data_buffer[data_length] = '\0';

            // Check for newline character
            char *newline_pos = strchr(data_buffer, '\n');
            while (newline_pos) {
                *newline_pos = '\0';

                // Write to file
                fputs(data_buffer, data_file);
                fputc('\n', data_file); 
                fflush(data_file);

                // Send the full content back to the client
                fseek(data_file, 0, SEEK_SET);
                size_t bytes_read;
                while ((bytes_read = fread(recv_buffer, 1, sizeof(recv_buffer), data_file)) > 0) {
                    send(client_fd, recv_buffer, bytes_read, 0);
                }

                // Move the remaining data to the beginning of the buffer
                size_t remaining_length = data_length - (newline_pos - data_buffer + 1);
                memmove(data_buffer, newline_pos + 1, remaining_length);
                data_length = remaining_length;
                data_buffer[data_length] = '\0'; 

                // Check for the next newline
                newline_pos = strchr(data_buffer, '\n');
            }
        }

        free(data_buffer);
        fclose(data_file);
        close(client_fd);

        // Log closed connection
        syslog(LOG_INFO, "Closed connection from %s", client_ip);
    }

    // Cleanup
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
    if (listen(server_fd, 0) != 0) {
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
