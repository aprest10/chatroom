#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <pthread.h>
#include <signal.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#define BUFFER 2048

volatile sig_atomic_t flag = 0;
int sockfd = 0;
char name[32];

void str_overwrite_stdout() {
    printf("\r%s", "> ");
    fflush(stdout);
}

void str_trim_lf(char *arr, int length) {
    for (int i = 0; i < length; ++i) {
        if (arr[i] == '\n') {
            arr[i] = '\0';
            break;
        }
    }
}

void catch_ctrl_c_and_exit() {
    flag = 1;
}

void recv_msg_handler() {
    char message[BUFFER] = {};

    while (1) {
        int receive = recv(sockfd, message, BUFFER, 0);

        if (receive > 0) {
            printf("%s", message);
            str_overwrite_stdout();
        }
        else if (receive == 0) {
            break;
        }
        memset(message, 0, sizeof(message));
    }
}

void send_msg_handler() {
    char message[BUFFER] = {};
    char buffer[BUFFER + 32] = {};

    while (1) {
        str_overwrite_stdout();
        fgets(message, BUFFER, stdin);
        str_trim_lf(message, BUFFER);

        if (strcmp(message, "exit") == 0) {
            break;
        }
        else {
            sprintf(buffer, "%s: %s\n", name, message);
            send(sockfd, buffer, strlen(buffer), 0);
        }

        bzero(message, BUFFER);
        bzero(buffer, BUFFER + 32);
    }
    catch_ctrl_c_and_exit();
}

int main(int argc, char *argv[]) {
    if (argc != 2) {
        printf("Usage: %s <port>\n", argv[0]);
        return EXIT_FAILURE;
    }

    char *ip = "127.0.0.1";
    int port = atoi(argv[1]);

    signal(SIGINT, catch_ctrl_c_and_exit);
    printf("Enter your name: ");
    fgets(name, 32, stdin);
    str_trim_lf(name, strlen(name));

    if (strlen(name) > 32 - 1 || strlen(name) < 2) {
        perror("Enter a name between 2 and 31 characters\n");
        return EXIT_FAILURE;
    }

    struct sockaddr_in server_addr;

    // socket settings
    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = inet_addr(ip);
    server_addr.sin_port = htons(port);

    // Connect to the server
    int err = connect(sockfd, (struct sockaddr*)&server_addr, sizeof(server_addr));

    if (err == -1) {
        perror("ERROR: connect\n");
        return EXIT_FAILURE;
    }

    // Send name
    send(sockfd, name, 32, 0);
    printf("Welcome to the chatroom!\n");

    pthread_t send_msg_thread;
    if (pthread_create(&send_msg_thread, NULL, (void *)send_msg_handler, NULL) != 0) {
        perror("ERROR: pthread\n");
        return EXIT_FAILURE;
    }

    pthread_t recv_msg_thread;
    if (pthread_create(&recv_msg_thread, NULL, (void *)recv_msg_handler, NULL) != 0) {
        perror("ERROR: pthread\n");
        return EXIT_FAILURE;
    }

    while (1) {
        if (flag) {
            printf("\nGood Bye! :)\n\n");
            break;
        }
    }
    close(sockfd);

    return EXIT_SUCCESS;
}