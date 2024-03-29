#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <pthread.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>

#define     PORT        6010
#define     MAXCLIENTS  5
#define     BUFLEN      1500
#define     ROOMFULL    "This room is full!\n"

struct s_client_info {
    pthread_t tid;
    int socket;
} clients[MAXCLIENTS];

void *service(void *arg) {
    struct s_client_info *p_clients = (struct s_client_info *)arg;
    char buf[BUFLEN] = {0};
    int len = 0;
    int ret = 0;
    int clients_cnt = 0;

    printf("[%d]: client service Entry-->\n", p_clients->socket);
    for (int i = 0; i < MAXCLIENTS; i++) {
        if (clients[i].socket != 0) {
            clients_cnt++;
        }
    }
    len = sprintf(buf, "There are %d/%d in this room.\n", clients_cnt, MAXCLIENTS);
    ret = send(p_clients->socket, buf, len, 0);
    if (ret == -1) {
        perror("send");
    }

    while (1) {
        len = recv(p_clients->socket, buf, BUFLEN, 0);
        buf[len] = 0;
        printf("[%d]: %s", p_clients->socket, buf);
        if (len > 0) {
            for (int i = 0; i < MAXCLIENTS; i++) {
                // boardcast ot other clients
                if (clients[i].socket != p_clients->socket && clients[i].socket != 0) {
                    eagain:
                    ret = send(clients[i].socket, buf, len, 0);
                    if (ret == -1) {
                        perror("send");
                        if (errno == EAGAIN || errno == EWOULDBLOCK) {
                            goto eagain;
                        } else {
                            continue;
                        }
                    }
                }
            }
        } else {
            close(p_clients->socket);
            p_clients->socket = 0;
            p_clients->tid = 0;
            printf("client service Exit-->\n");
            return NULL;
        }
    }
}

int main(int argc, char *argv[]) {
    int ret;

    int listen_sock = -1;
    int connect_sock = -1;

    pthread_attr_t attr;

    ret = pthread_attr_init(&attr);
    if (ret != 0) {
        perror("pthread_attr_init");
    }

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(PORT);
    addr.sin_addr.s_addr = htonl(INADDR_ANY);

    listen_sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (listen_sock == -1) {
        perror("socket");
    }

    ret = bind( listen_sock, (struct sockaddr*)&addr, sizeof(addr));
    if (ret == -1)  {
        perror("bind");
        close(listen_sock);
        return 1;
    }

    ret = listen(listen_sock, SOMAXCONN);
    if (ret == -1) {
        perror("listen");
        return 1;
    }

    printf("wait for clients\n");

    while (1) {
        loop_start:
        connect_sock = accept(listen_sock, NULL, NULL);
        if (connect_sock == -1) {
            perror("accept");
            continue;
        }

        for (int i = 0; i < MAXCLIENTS; i++) {
            if (clients[i].tid == 0) {
                printf("client connected: %d\n", connect_sock);
                clients[i].socket = connect_sock;
                pthread_create(&clients[i].tid, &attr, &service, &clients[i]);
                goto loop_start;
            }
        }

        printf("max clients reached\n");
        ret = send(connect_sock, ROOMFULL, sizeof(ROOMFULL), 0);
        if (ret == -1) {
            perror("send");
        }
        close(connect_sock);
    }

    for (int i = 0; i < MAXCLIENTS; i++) {
        pthread_join(clients[i].tid, NULL);
    }

    close(listen_sock);

    return 0;
}