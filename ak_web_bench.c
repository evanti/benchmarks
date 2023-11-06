/*
    This program is designed to test the performance of a network, with a particular focus on the Linux TCP stack. It operates in two modes: server and client. 

    In server mode, the program opens a specified number of sockets (-t) on ports starting from the base port # 10,000. If a port is already in use, the program will display an error but will not terminate. If this occurs, it is recommended to stop the program and find another base port that allows for the opening of -t sequential ports. The server mode does not record any statistics. 

    In client mode, the program creates a specified number of connections (-t) to the server, using server ports starting from 10,000. The client mode records and outputs the following statistics every second: total number of threads running, total number of operations performed, and average number of operations per thread. 

    The program does not terminate on its own and must be manually stopped using Ctrl-C. When the client is stopped, the server continues to run and can be used for more tests.

    To compile the program, use the following command: gcc -o web_bench ak_web_bench.c -lpthread 

    Usage:
    Server mode: ./web_bench -s -t <num_threads>
    Client mode: ./web_bench -c <server_ip> -t <num_threads>

    The load on the server and the client from this benchmark is very much the same. So if your server VM's CPU/Network are less powerful, you won't get proper results on the client VM.
*/

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <pthread.h>
#include <time.h>
#include <stdbool.h>
#include <signal.h>

#define SERVER_IP_LENGTH 16
char SERVER_IP[SERVER_IP_LENGTH] = "127.0.0.1";
#define BASE_PORT 10000
#define BUFFER_SIZE 1024

// Global variables
int total_operations = 0;
pthread_mutex_t lock;

// Show usage information
void show_usage() {
    printf("Usage: web_bench [-s] [-c <server_ip>] [-t <num_threads>]\n");
    printf("  -s: run as server\n");
    printf("  -c <server_ip>: run as client and connect to server at <server_ip>\n");
    printf("  -t <num_threads>: number of threads to use (default: 1)\n");
}

// Client thread function
void *client_thread_func(void *arg) {
    int port = *(int *)arg;
    int sock;
    struct sockaddr_in server;
    char message[BUFFER_SIZE] , server_reply[BUFFER_SIZE];

    // Create socket
    sock = socket(AF_INET , SOCK_STREAM , 0);
    if (sock == -1) {
        printf("Could not create socket");
    }

    // Set up server address
    server.sin_addr.s_addr = inet_addr(SERVER_IP);
    server.sin_family = AF_INET;
    server.sin_port = htons(port);

    // Connect to remote server
    if (connect(sock , (struct sockaddr *)&server , sizeof(server)) < 0) {
        perror("connect failed. Error");
        return 0;
    }

    // Send some data
    strcpy(message, "GET / HTTP/1.1\r\n\r\n");
    while(1) {
        if(send(sock , message , strlen(message) , 0) < 0) {
            puts("Send failed");
            return 0;
        }

        // Receive a reply from the server
        if(recv(sock , server_reply , BUFFER_SIZE , 0) < 0) {
            puts("recv failed");
            break;
        }
        pthread_mutex_lock(&lock);
        total_operations++;
        pthread_mutex_unlock(&lock);
    }
    close(sock);
    return 0;
}

void *handle_connection(void *arg) {
    int new_socket = *(int *)arg;
    const char *response = "HTTP/1.1 200 OK\nContent-Type: text/plain\nContent-Length: 12\n\nHello World!";
    char buffer[1024] = {0};
    int valread;

    while (1) {
        memset(buffer, 0, sizeof(buffer));
        
        // Read data from the client
        valread = read(new_socket, buffer, 1024);
        if (valread == 0) {
            // Connection closed by client, break the loop
            break;
        } else if (valread < 0) {
            // Read error, continue to next iteration
            perror("Read failed");
            continue;
        }

        // Send response to client
        send(new_socket, response, strlen(response), 0);
    }

    // Close the connection
    close(new_socket);
    return NULL;
}

void *server_thread_func(void *arg) {
    int server_fd;
    struct sockaddr_in address;
    int addrlen = sizeof(address);
    int port = *((int *)arg);

    // Create socket
    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0) {
        perror("Socket creation failed");
        return NULL;
    }

    // Set socket options
    int opt = 1;
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, &opt, sizeof(opt))) {
        perror("Setsockopt failed");
        close(server_fd);
        return NULL;
    }

    // Set up server address
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(port);

    // Bind socket to address and port
    if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0) {
        perror("Bind failed");
        close(server_fd);
        return NULL;
    }

    // Start listening for incoming connections
    if (listen(server_fd, 3) < 0) {
        perror("Listen failed");
        close(server_fd);
        return NULL;
    }

    printf("Server listening on port %d\n", port);

    while (1) {
        // Accept incoming connection
        int new_socket;
        if ((new_socket = accept(server_fd, (struct sockaddr *)&address, (socklen_t *)&addrlen)) < 0) {
            perror("Accept failed");
            close(server_fd);
            return NULL;
        }

        // Handle the connection directly in this thread
        if (handle_connection(&new_socket) != 0) {
            perror("Connection handling failed");
            close(new_socket);
        }
    }

    return NULL;
}

int main(int argc, char *argv[]) {
    setbuf(stdout, NULL);
    int num_threads = 1;
    bool is_server = false;
    bool is_client = false;

    // Initialize mutex
    if (pthread_mutex_init(&lock, NULL) != 0) {
        printf("\n mutex init has failed\n");
        return 1;
    }

    // Parse command line arguments
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-t") == 0 && i + 1 < argc) {
            num_threads = atoi(argv[i + 1]);
            i++; // Skip next argument
        } else if (strcmp(argv[i], "-s") == 0) {
            is_server = true;
        } else if (strcmp(argv[i], "-c") == 0 && i + 1 < argc) {
            is_client = true;
            strncpy(SERVER_IP, argv[i + 1], SERVER_IP_LENGTH - 1);
            SERVER_IP[SERVER_IP_LENGTH - 1] = '\0'; // Ensure null-termination
            i++; // Skip next argument
        } else {
            show_usage();
            return 1;
        }
    }

    pthread_t threads[num_threads];
    int ports[num_threads];

    // Start server or client threads
    if (is_server) {
        for (int i = 0; i < num_threads; i++) {
            ports[i] = BASE_PORT + i;

            if (pthread_create(&threads[i], NULL, server_thread_func, &ports[i]) != 0) {
                perror("Thread creation failed");
                exit(EXIT_FAILURE);
            }
        }
    } else if (is_client) {
        for (int i = 0; i < num_threads; i++) {
            ports[i] = BASE_PORT + i;
            if (pthread_create(&threads[i], NULL, client_thread_func, &ports[i]) < 0) {
                perror("could not create thread");
                return 1;
            }
        }
    } else {
        show_usage();
        return 1;
    }

    // Print total number of operations every second
    time_t start_time = time(NULL);
    bool is_first = true;
    if (is_client) {
        printf("Threads,operations,ops_per_thread\n");
        while (1) {
            if (time(NULL) > start_time) {
                pthread_mutex_lock(&lock);
                if (!is_first) {
                    printf("%d,%d,%d\n", num_threads, total_operations, total_operations / num_threads);}
                total_operations = 0; 
                pthread_mutex_unlock(&lock);
                start_time = time(NULL);
                is_first = false;
            }
        }
    } else if (is_server) {
        // Monitor threads and create new threads if any thread exits
        while (1) {
            for (int i = 0; i < num_threads; i++) {
                if (pthread_kill(threads[i], 0) != 0) {
                    // Thread has exited, create a new thread on the same port
                    if (pthread_create(&threads[i], NULL, server_thread_func, &ports[i]) != 0) {
                        perror("Thread creation failed");
                        exit(EXIT_FAILURE);
                    }
                }
            }
        }
    }

    // Join threads
    for (int i = 0; i < num_threads; i++) {
        pthread_join(threads[i], NULL);
    }

    pthread_mutex_destroy(&lock);

    return 0;
}