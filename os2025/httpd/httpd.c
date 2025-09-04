#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <time.h>
#include <errno.h>
#include <signal.h>

// Don't include these in another file.
#include "thread.h"
#include "thread-sync.h"

#define BUFFER_SIZE 4096
#define MAX_PATH_LENGTH 1024
#define DEFAULT_PORT 8080
#define MAX_QUEUE 1024

extern char** environ;

// typedef struct {
//     int client_socket;
//     int request_id;
// } Task;

// Task task_queue[MAX_QUEUE];
// int queue_head = 0;
// int queue_tail = 0;

// mutex_t queue_mutex = MUTEX_INIT();
// cond_t queue_cond = COND_INIT();
// sem_t queue_sem;

// int request_counter = 0;
// mutex_t request_counter_mutex = MUTEX_INIT();

// int current_log_number = 0;
// mutex_t log_mutex = MUTEX_INIT();
// cond_t log_cond = COND_INIT();

// Revise this.
void handle_request(int client_socket, char *method, char *path, int *status_code);

void send_error_response(int client_socket, int status_code);

void send_static_response(int client_socket);

void worker(int tid);

// Call this.
void log_request(const char *method, const char *path, int status_code);

// Socket variables
int server_socket, client_socket;
struct sockaddr_in server_addr, client_addr;
socklen_t client_len = sizeof(client_addr);

int main(int argc, char *argv[]) {
    // Socket variables
    // int server_socket, client_socket;
    // struct sockaddr_in server_addr, client_addr;
    // socklen_t client_len = sizeof(client_addr);
    
    // Get port from command line or use default
    int port = (argc > 1) ? atoi(argv[1]) : DEFAULT_PORT;

    // Set up signal handler for SIGPIPE to prevent crashes
    // when client disconnects
    signal(SIGPIPE, SIG_IGN);

    // Create socket
    if ((server_socket = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        perror("Socket creation failed");
        exit(EXIT_FAILURE);
    }

    // Set socket options to reuse address
    // (prevents "Address already in use" errors)
    int opt = 1;
    if (setsockopt(server_socket, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
        perror("Setsockopt failed");
        exit(EXIT_FAILURE);
    }

    // Configure server address
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;         // IPv4
    server_addr.sin_addr.s_addr = INADDR_ANY; // Accept connections on any interface
    server_addr.sin_port = htons(port);       // Convert port to network byte order

    // Bind socket to address and port
    if (bind(server_socket, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("Bind failed");
        exit(EXIT_FAILURE);
    }

    // Listen for incoming connections with system-defined maximum backlog
    if (listen(server_socket, SOMAXCONN) < 0) {
        perror("Listen failed");
        exit(EXIT_FAILURE);
    }

    printf("Server listening on port %d...\n", port);

    // SEM_INIT(&queue_sem, 0);

    for (int i = 0; i < 16; i++) {
        // Create worker threads
        spawn(worker);
    }

    // Main server loop - accept and process connections indefinitely
    /*while (1) {
        // Accept new client connection
        // if ((client_socket = accept(server_socket,
        //                             (struct sockaddr *)&client_addr,
        //                             &client_len)) < 0) {
        //     perror("Accept failed");
        //     continue;  // Continue listening for other connections
        // }

        // mutex_lock(&request_counter_mutex);
        // int req_num = ++request_counter;
        // mutex_unlock(&request_counter_mutex);

        // mutex_lock(&queue_mutex);
        // task_queue[queue_tail % MAX_QUEUE] = (Task){client_socket, req_num};
        // queue_tail++;
        // V(&queue_sem);
        // mutex_unlock(&queue_mutex);

        // Set timeouts to prevent hanging on slow or dead connections
        struct timeval timeout;
        timeout.tv_sec = 30;  // 30 seconds timeout
        timeout.tv_usec = 0;
        setsockopt(client_socket, SOL_SOCKET, SO_RCVTIMEO,
                   (const char*)&timeout, sizeof(timeout));
        setsockopt(client_socket, SOL_SOCKET, SO_SNDTIMEO,
                   (const char*)&timeout, sizeof(timeout));

        // Process the client request
        handle_request(client_socket);
    }*/

    // Clean up (note: this code is never reached in this example)
    // close(server_socket);
    return 0;
}

void handle_request(int client_socket, char *method, char *path, int *status_code) {
    char buffer[BUFFER_SIZE];
    int bytes_received;

    // Read request
    bytes_received = recv(client_socket, buffer, BUFFER_SIZE - 1, 0);
    if (bytes_received <= 0) {
        *status_code = 400; // Bad Request
        send_error_response(client_socket, *status_code);
        close(client_socket);
        return;
    }
    buffer[bytes_received] = '\0';

    // printf("Got a new request:\n%s\n", buffer);

    char *request_line = strtok(buffer, "\r\n");
    if (request_line == NULL) {
        *status_code = 400; // Bad Request
        send_error_response(client_socket, *status_code);
        close(client_socket);
        return;
    }

    sscanf(request_line, "%s %s", method, path);

    if (strncmp(path, "/cgi-bin/", 9) == 0) {
        char *query_string = strchr(path, '?');
        if (query_string) {
            *query_string = '\0'; // Split path and query string
        }
        char script_path[MAX_PATH_LENGTH];
        snprintf(script_path, sizeof(script_path), ".%s", path);
        if (strstr(path, "..")) {
            *status_code = 404; // Not Found
            send_error_response(client_socket, *status_code);
            close(client_socket);
            return;
        }

        struct stat st;
        if (stat(script_path, &st) < 0 || !S_ISREG(st.st_mode) || access(script_path, X_OK) < 0) {
            *status_code = (errno == ENOENT) ? 404 : 500; // Not Found or Internal Server Error
            send_error_response(client_socket, *status_code);
            close(client_socket);
            return;
        }

        char* e1 = (char*)malloc(366 * sizeof(char));
        char* e2 = (char*)malloc(366 * sizeof(char));
        sprintf(e1, "REQUEST_METHOD=%s", method);
        sprintf(e2, "QUERY_STRING=%s", query_string ? query_string + 1 : "");

        environ[0] = e1; // Clear previous method
        environ[1] = e2; // Clear previous query string
        environ[2] = NULL; // Terminate the environment array
        // setenv("REQUEST_METHOD", method, 1);
        // setenv("QUERY_STRING", query_string ? query_string + 1 : "", 1); 

        int pipe_fd[2];
        if (pipe(pipe_fd) < 0) {
            *status_code = 500; // Internal Server Error
            send_error_response(client_socket, *status_code);
            close(client_socket);
            return;
        }

        pid_t pid = fork();
        if (pid < 0) {
            *status_code = 500; // Internal Server Error
            send_error_response(client_socket, *status_code);
            close(pipe_fd[0]);
            close(pipe_fd[1]);
            close(client_socket);
            return;
        } else if (pid == 0) { // Child process
            dup2(pipe_fd[1], STDOUT_FILENO);
            close(pipe_fd[0]);
            close(pipe_fd[1]);
            execl(script_path, script_path, NULL);
            perror("execl failed");
            exit(1);
        } else { // Parent process
            close(pipe_fd[1]);
            send(client_socket, "HTTP/1.1 200 OK\r\nContent-Type: text/html\r\nConnection: close\r\n\r\n", 58, 0);
            char cgi_buffer[BUFFER_SIZE];
            ssize_t bytes_read;
            int STATUS = 200; // Default status code
            bytes_read = read(pipe_fd[0], cgi_buffer, sizeof(cgi_buffer) - 1);
            cgi_buffer[bytes_read] = '\0'; // Null-terminate the buffer
            sscanf(cgi_buffer, "%*s %d", &STATUS); // Extract status code from CGI output
            send(client_socket, cgi_buffer, bytes_read, 0);
            while ((bytes_read = read(pipe_fd[0], cgi_buffer, sizeof(cgi_buffer) - 1)) > 0) {
                cgi_buffer[bytes_read] = '\0';
                send(client_socket, cgi_buffer, bytes_read, 0);
            }
            close(pipe_fd[0]);
            int status;
            waitpid(pid, &status, 0); // Wait for child process to finish
            *status_code = WIFEXITED(status) ? STATUS : 500; // Check exit status
        }
    }
    else {
        send_static_response(client_socket);;
        *status_code = 200; // OK
    }

    // Close the connection
    close(client_socket);
}

void send_error_response(int client_socket, int status_code) {
    const char *status_phrase = "";
    switch (status_code) {
        case 400: status_phrase = "Bad Request"; break;
        case 404: status_phrase = "Not Found"; break;
        case 500: status_phrase = "Internal Server Error"; break;
        default: status_phrase = "Unknown Error"; break;
    }
    char response[512];
    snprintf(response, sizeof(response),
             "HTTP/1.1 %d %s\r\n"
             "Content-Type: text/plain\r\n"
             "Connection: close\r\n"
             "\r\n"
             "%d %s",
             status_code, status_phrase, status_code, status_phrase);
    send(client_socket, response, strlen(response), 0);
}

void send_static_response(int client_socket) {
    const char *response_body = "Under construction";
    int body_length = strlen(response_body);
    char content_length_header[64];
    sprintf(content_length_header, "Content-Length: %d\r\n", body_length);
    send(client_socket, "HTTP/1.1 200 OK\r\n", 17, 0);
    send(client_socket, "Content-Type: text/plain\r\n", 26, 0);
    send(client_socket, content_length_header, strlen(content_length_header), 0);
    send(client_socket, "Connection: close\r\n\r\n", 19, 0);
    send(client_socket, response_body, body_length, 0);
}

void worker(int tid) {
    while (1) {
        if ((client_socket = accept(server_socket,
                                    (struct sockaddr *)&client_addr,
                                    &client_len)) < 0) {
            perror("Accept failed");
            continue;  // Continue listening for other connections
        }
        // P(&queue_sem);
        // mutex_lock(&queue_mutex);
        // Task task = task_queue[queue_head % MAX_QUEUE];
        // queue_head++;
        // mutex_unlock(&queue_mutex);

        char method[16], path[MAX_PATH_LENGTH];
        int status_code = 0;

        handle_request(client_socket, method, path, &status_code);

        // Log the request
        // mutex_lock(&log_mutex);
        // while (current_log_number + 1 != task.request_id) {
        //     cond_wait(&log_cond, &log_mutex);
        // }
        log_request(method, path, status_code);
        // current_log_number++;
        // cond_broadcast(&log_cond);
        // mutex_unlock(&log_mutex);

        close(client_socket);
    }
}

void log_request(const char *method, const char *path, int status_code) {
    time_t now;
    struct tm *tm_info;
    char timestamp[26];

    time(&now);
    tm_info = localtime(&now);
    strftime(timestamp, 26, "%Y-%m-%d %H:%M:%S", tm_info);

    // In real systems, we write to a log file,
    // like /var/log/nginx/access.log
    printf("[%s] [%s] [%s] [%d]\n", timestamp, method, path, status_code);
    fflush(stdout);
}
