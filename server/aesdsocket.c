#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>
#include <stdio.h>
#include <syslog.h>
#include <signal.h>
#include <arpa/inet.h>
#include <limits.h>
#include <errno.h>

volatile sig_atomic_t graceful_exit = 0;

void signal_handler(int signum) 
{
    graceful_exit = 1;
}


int main(int argc, char *argv[]) {
    int status;
    struct sockaddr_storage their_addr;
    socklen_t addr_size;
    struct addrinfo hints;
    struct addrinfo *servinfo;
    int sockfd;
    int new_sockfd;
    int ret;

    int daemon_mode = 0;
    int opt;
    while ((opt = getopt(argc, argv, "d")) != -1) {
        if (opt == 'd') {
            daemon_mode = 1;
        }
    }
   

    // signal handling
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);

    // Initialize syslog
    openlog("aesdsocket", LOG_PID | LOG_CONS, LOG_USER);
    
   
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC;     // IPv4 or IPv6
    hints.ai_socktype = SOCK_STREAM; // TCP stream sockets
    hints.ai_flags = AI_PASSIVE; //take localhost address

    // call getaddrinfo function
    if((status =getaddrinfo(NULL, "9000", &hints, &servinfo)) != 0) {
        syslog(LOG_ERR, "getaddrinfo error: %s", gai_strerror(status));
        //close(fd);
        freeaddrinfo(servinfo);
        exit(EXIT_FAILURE);
    }

    sockfd = socket(servinfo->ai_family, servinfo->ai_socktype, servinfo->ai_protocol);
    if(sockfd == -1) {
        syslog(LOG_ERR, "socket error");
        //close(fd);
        freeaddrinfo(servinfo);
        exit(EXIT_FAILURE);
    }

    // lose the pesky "Address already in use" error message
    int yes = 1;
    setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof yes);

    int retry_count = 0;
    const int max_retries = 10;
    const int retry_delay = 1; // seconds
    int bound = 0;
    
    while(retry_count < max_retries)
    {
        ret = bind(sockfd, servinfo->ai_addr, servinfo->ai_addrlen);
        if(ret == 0)
        {
            syslog(LOG_USER, "Successfully bound to port 9000");
            bound = 1;
            break; // success
        }
        else if(errno == EADDRINUSE) {
            syslog(LOG_ERR, "port in use bind error");
            sleep(retry_delay);
            retry_count++;
        }
        else
        {
            //some other error
            syslog(LOG_ERR, "bind error on port 9000: %m"); // %m to print errno message
            break;
            

        }
    }
    
    if(!bound) 
    {
        //close(fd);
        freeaddrinfo(servinfo);
        exit(EXIT_FAILURE);
    }
    


    //listen for incoming connections
    ret = listen(sockfd, 10);
    if(ret == -1) {
        syslog(LOG_ERR, "listen error");
        //close(fd);
        freeaddrinfo(servinfo);
        exit(EXIT_FAILURE);
    }

     // open a file for writing
    int fd = open("/var/tmp/aesdsocketdata", O_CREAT | O_APPEND | O_TRUNC | O_SYNC | O_RDWR, 0666);
    if(fd == -1) {
        perror("open");
        exit(EXIT_FAILURE);
    }

    close(fd); // close and reopen in the loop to ensure data is written properly

    //daemonize if -d option is provided
    if(daemon_mode) {
        pid_t pid =fork();
        if(pid < 0) {
            syslog(LOG_ERR, "fork error");
            close(fd);
            freeaddrinfo(servinfo);
            exit(EXIT_FAILURE);
        }
        if (pid > 0) {
            // parent process
            exit(EXIT_SUCCESS);
        }
        // child process continues
        // fork again for deamon
        pid = fork();
        if(pid < 0) {
            syslog(LOG_ERR, "fork error");
            close(fd);
            freeaddrinfo(servinfo);
            exit(EXIT_FAILURE);
        }
        if (pid > 0) {
            // parent process
            exit(EXIT_SUCCESS);
        }
        // child process continues
        //change workdir to root
        if(chdir("/") < 0) {
            syslog(LOG_ERR, "chdir error");
            close(fd);
            freeaddrinfo(servinfo);
            exit(EXIT_FAILURE);
        }
        //close standard file descriptors
        close(STDIN_FILENO);
        close(STDOUT_FILENO);
        close(STDERR_FILENO);

        // redirect to standard output
        int fd_null = open("/dev/null", O_RDWR);
        if(fd_null != -1) {
            dup2(fd_null, STDIN_FILENO);
            dup2(fd_null, STDOUT_FILENO);
            dup2(fd_null, STDERR_FILENO);
            close(fd_null);
        } else {
            syslog(LOG_ERR, "/dev/null open error");
            close(fd);
            freeaddrinfo(servinfo);
            exit(EXIT_FAILURE);
        }
    }

    //accept a connection in a loop till SIGINT or SIGTERM is received
    while(!graceful_exit)
    {
        addr_size = sizeof(their_addr);
        new_sockfd = accept(sockfd, (struct sockaddr *)&their_addr, &addr_size);
        if(new_sockfd == -1) {
            syslog(LOG_ERR, "accept error");
            if (errno == EINTR) 
            {
            // The signal handler was called; the loop condition will now see graceful_exit=1
            break; 
            }
            close(fd);
            freeaddrinfo(servinfo);
            exit(EXIT_FAILURE);
        }
        
        char host[NI_MAXHOST];
        getnameinfo((struct sockaddr *)&their_addr, addr_size, host, sizeof(host), NULL, 0, NI_NUMERICHOST);
        syslog(LOG_INFO, "Accepted connection from %s", host);
    

        size_t current_buf_size = 1024;
        char *buffer = malloc(current_buf_size);
        size_t total_recv = 0;

        

        while (!graceful_exit) 
        {
            ssize_t bytes_received = recv(new_sockfd, buffer + total_recv, current_buf_size - total_recv - 1, 0);
            if (bytes_received < 0) 
            {
                if(errno == EINTR)
                {
                    // Interrupted by signal
                    break;
                }
                syslog(LOG_ERR, "recv error");
                break;
            }

            total_recv += bytes_received;
            buffer[total_recv] = '\0';

            // Check for newline
            if (total_recv > 0 && memchr(buffer, '\n', total_recv) != NULL) 
            {
                // Write received data to file
                // \n is found. now open the file
                int data_fd = open("/var/tmp/aesdsocketdata", O_CREAT | O_APPEND | O_RDWR, 0666);
                if (data_fd != -1) 
                {
                
                    write(data_fd, buffer, total_recv);
                    fsync(data_fd);
                    
                    //  readback
                    lseek(data_fd, 0, SEEK_SET); // Reset file offset to beginning
                    char read_buf[1024];
                    ssize_t bytes_read;
                    
                    while ((bytes_read = read(data_fd, read_buf, sizeof(read_buf))) > 0) {
                        send(new_sockfd, read_buf, bytes_read, 0);
                    }
                    close(data_fd);
                }
                break; // Finished this transaction
            }
            if(bytes_received == 0) {
                // Connection closed by client
                break;
            }  

            // Expand buffer if needed
            current_buf_size += 1024;
            char *tmp = realloc(buffer, current_buf_size);
            if (tmp == NULL) {
                free(buffer);
                // Handle error
                return -1;
            }
            buffer = tmp;
        }
        free(buffer);
        close(new_sockfd);
        syslog(LOG_INFO, "Closed connection from %s", host);

    }


    //clean up and close
    
    syslog(LOG_INFO, "Caught signal, exiting");
    close(sockfd);
    remove("/var/tmp/aesdsocketdata");
    freeaddrinfo(servinfo);
    closelog();
    return 0;
}



