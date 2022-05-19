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

#define MAX_CLIENTS 100
#define BUFFER 2048

static _Atomic unsigned int cli_count = 0;
static int uid = 10;

typedef struct {
    struct sockaddr_in address;
    int sockfd;
    int uid;
    char name[32];
} client_t;

client_t *clients[MAX_CLIENTS];

pthread_mutex_t clients_mutex = PTHREAD_MUTEX_INITIALIZER;

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

void print_client_addr(struct sockaddr_in addr) {
    printf("%d.%d.%d.%d",
           addr.sin_addr.s_addr & 0xff,
           (addr.sin_addr.s_addr & 0xff00) >> 8,
           (addr.sin_addr.s_addr & 0xff0000) >> 16,
           (addr.sin_addr.s_addr & 0xff000000) >> 24);
}

void queue_add(client_t *cl) {
    pthread_mutex_lock(&clients_mutex);

    for (int i = 0; i < MAX_CLIENTS; ++i) {
        if (!clients[i]) {
            clients[i] = cl;
            break;
        }
    }

    pthread_mutex_unlock(&clients_mutex);
}

void queue_remove(int temp_uid) {
    pthread_mutex_lock(&clients_mutex);

    for (int i = 0; i < MAX_CLIENTS; ++i) {
        if (clients[i]) {
            if (clients[i] -> uid == temp_uid) {
                clients[i] = NULL;
                break;
            }
        }
    }

    pthread_mutex_unlock(&clients_mutex);
}

void send_message(char *s, int temp_uid) {
    pthread_mutex_lock(&clients_mutex);

    for (int i = 0; i < MAX_CLIENTS; ++i) {
        if (clients[i]) {
            if (clients[i] -> uid != temp_uid) {
                if (write(clients[i] -> sockfd, s, strlen(s)) < 0) {
                    printf("ERROR: write to descriptor failed\n");
                    break;
                }
            }
        }
    }

    pthread_mutex_unlock(&clients_mutex);
}

void *handle_client(void *arg) {
    char buffer[BUFFER];
    char name[32];
    int leave_flag = 0;
    cli_count++;

    client_t *cli = (client_t*)arg;

    // name
    if (recv(cli->sockfd, name, 32, 0) <= 0 || strlen(name) < 2 || strlen(name) >= 31) {
        printf("Didn't enter a proper name\n");
        leave_flag = 1;
    }
    else {
        strcpy(cli->name, name);
        sprintf(buffer, "%s has joined the chat\n", cli->name);
        printf("%s", buffer);
        send_message(buffer, cli->uid);
    }

    bzero(buffer, BUFFER);

    while (1) {
        if (leave_flag) {
            break;
        }

        int receive = recv(cli->sockfd, buffer, BUFFER, 0);

        if (receive > 0) {
            if (strlen(buffer) > 0) {
                send_message(buffer, cli->uid);
                str_trim_lf(buffer, strlen(buffer));
                printf("%s -> %s\n", buffer, cli->name);
            }
        }
        else if (receive == 0 || strcmp(buffer, "exit") == 0) {
            sprintf(buffer, "%s has left the chat\n", cli->name);
            printf("%s", buffer);
            send_message(buffer, cli->uid);
            leave_flag = 1;
        }
        else {
            printf("ERROR: -1\n");
            leave_flag = 1;
        }

        bzero(buffer, BUFFER);
    }

    close(cli->sockfd);
    queue_remove(cli->uid);
    free(cli);
    cli_count--;
    pthread_detach(pthread_self());

    return NULL;
}

int main(int argc, char *argv[]) {
    if (argc != 2) {
        printf("Usage: %s <port>\n", argv[0]);
        return EXIT_FAILURE;
    }

    char *ip = "127.0.0.1";
    int port = atoi(argv[1]);

    int option = 1;
    int listenfd = 0;
    int connfd = 0;
    struct sockaddr_in serv_addr;
    struct sockaddr_in cli_addr;
    pthread_t tid;

    // socket settings
    listenfd = socket(AF_INET, SOCK_STREAM, 0);
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = inet_addr(ip);
    serv_addr.sin_port = htons(port);

    // signals
    signal(SIGPIPE, SIG_IGN);

    if (setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR, (char*)&option, sizeof(option)) < 0) {
        perror("ERROR: setsocketopt\n");
        return EXIT_FAILURE;
    }

    // bind
    if (bind(listenfd, (struct sockaddr*)&serv_addr, sizeof(serv_addr)) < 0) {
        perror("ERROR: bind\n");
        return EXIT_FAILURE;
    }

    // listen
    if (listen(listenfd, 10) < 0) {
        perror("ERROR: listen\n");
        return EXIT_FAILURE;
    }

    printf("This is the chatroom server\n");

    while (1) {
        socklen_t clilen = sizeof (cli_addr);
        connfd = accept(listenfd, (struct sockaddr*)&cli_addr, &clilen);

        // check for max clients
        if ((cli_count+1) == MAX_CLIENTS) {
            printf("Max clients connected\nRejected\n");
            print_client_addr(cli_addr);
            printf(":%d\n", cli_addr.sin_port);
            close(connfd);
            continue;
        }

        // client settings
        client_t *cli = (client_t *) malloc(sizeof(client_t));
        cli->address = cli_addr;
        cli->sockfd = connfd;
        cli->uid = uid++;

        // add clients to queue
        queue_add(cli);
        pthread_create(&tid, NULL, &handle_client, (void *)cli);

        // reduce CPU usage
        sleep(1);
    }

    return EXIT_SUCCESS;
}
