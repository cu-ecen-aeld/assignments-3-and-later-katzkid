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
#include <errno.h>

volatile sig_atomic_t graceful_exit = 0;
void signal_handler(int signum) { graceful_exit = 1; }

// Helper function to ensure everything is sent even if Valgrind throttles the socket
int send_all(int s, char *buf, int len) {
    int total = 0;
    int bytesleft = len;
    int n;
    while(total < len) {
        n = send(s, buf + total, bytesleft, 0);
        if (n == -1) break;
        total += n;
        bytesleft -= n;
    }
    return n == -1 ? -1 : 0;
}

int main(int argc, char *argv[]) {
    struct sockaddr_storage their_addr;
    socklen_t addr_size;
    struct addrinfo hints, *servinfo;
    int sockfd, new_sockfd;
    int daemon_mode = 0;

    // Check for daemon flag
    int opt;
    while ((opt = getopt(argc, argv, "d")) != -1) {
        if (opt == 'd') daemon_mode = 1;
    }

    // Set up signals
    struct sigaction sa = {.sa_handler = signal_handler};
    sigaction(SIGINT, &sa, NULL);
    sigaction(SIGTERM, &sa, NULL);

    openlog("aesdsocket", LOG_PID, LOG_USER);

    // Initial Cleanup: Ensure file is empty and fresh
    int init_fd = open("/var/tmp/aesdsocketdata", O_CREAT | O_TRUNC | O_WRONLY, 0666);
    if (init_fd != -1) close(init_fd);

    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET; // Force IPv4 for simpler testing
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;

    if (getaddrinfo(NULL, "9000", &hints, &servinfo) != 0) return -1;

    sockfd = socket(servinfo->ai_family, servinfo->ai_socktype, servinfo->ai_protocol);
    int yes = 1;
    setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int));

    if (bind(sockfd, servinfo->ai_addr, servinfo->ai_addrlen) != 0) {
        freeaddrinfo(servinfo);
        return -1;
    }
    freeaddrinfo(servinfo);

    if (listen(sockfd, 10) == -1) return -1;

    // Daemonize AFTER successful bind
    if (daemon_mode) {
        if (fork() > 0) exit(0);
        setsid();
        if (fork() > 0) exit(0);
        chdir("/");
        int fd_null = open("/dev/null", O_RDWR);
        dup2(fd_null, STDIN_FILENO);
        dup2(fd_null, STDOUT_FILENO);
        dup2(fd_null, STDERR_FILENO);
        close(fd_null);
    }

    while (!graceful_exit) {
        addr_size = sizeof(their_addr);
        new_sockfd = accept(sockfd, (struct sockaddr *)&their_addr, &addr_size);
        if (new_sockfd == -1) {
            if (errno == EINTR) break;
            continue;
        }

        char host[NI_MAXHOST];
        getnameinfo((struct sockaddr *)&their_addr, addr_size, host, sizeof(host), NULL, 0, NI_NUMERICHOST);
        syslog(LOG_INFO, "Accepted connection from %s", host);

        size_t buf_size = 1024;
        char *buffer = malloc(buf_size);
        size_t total_recv = 0;

        while (!graceful_exit) 
        {
            ssize_t nr = recv(new_sockfd, buffer + total_recv, buf_size - total_recv - 1, 0);
            if (nr <= 0) break;
            total_recv += nr;
            buffer[total_recv] = '\0';

            if (memchr(buffer, '\n', total_recv)) {
                
                int data_fd = open("/var/tmp/aesdsocketdata", O_WRONLY | O_APPEND | O_CREAT, 0666);
                write(data_fd, buffer, total_recv);
                fsync(data_fd); // Crucial for Valgrind
                close(data_fd); // Forces metadata sync

                data_fd = open("/var/tmp/aesdsocketdata", O_RDONLY);
                char r_buf[1024];
                ssize_t r_bytes;
                while ((r_bytes = read(data_fd, r_buf, sizeof(r_buf))) > 0) {
                    send_all(new_sockfd, r_buf, r_bytes);
                }
                close(data_fd);
                
                total_recv = 0; // reset for next potential line
                break; // Break after each newline
            }

            if (total_recv >= buf_size - 1) {
                buf_size += 1024;
                buffer = realloc(buffer, buf_size);
            }
        }
        free(buffer);
        close(new_sockfd);
        syslog(LOG_INFO, "Closed connection from %s", host);
    }

    syslog(LOG_INFO, "Caught signal, exiting");
    close(sockfd);
    remove("/var/tmp/aesdsocketdata");
    closelog();
    return 0;
}