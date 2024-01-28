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

struct s_client_info {
    pthread_t tid;
    int socket;
} clients[MAXCLIENTS];

void *service(void *arg) {
    struct s_client_info *p_clients = (struct s_client_info *)arg;
    char buf[BUFLEN];
    int len;
    int ret;

    printf("client service Entry--> [%ld]%d\n", p_clients->tid, p_clients->socket);

    memset(buf, 0, BUFLEN);

    while (1) {
        len = recv(p_clients->socket, buf, BUFLEN, 0);
        printf("[%ld]%d: %s\n", p_clients->tid, p_clients->socket, buf);
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
        } else if (len == 0) {
            return NULL;
        } else {
            close(p_clients->socket);
            p_clients->socket = 0;
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

        close(connect_sock);
    }

    for (int i = 0; i < MAXCLIENTS; i++) {
        pthread_join(clients[i].tid, NULL);
    }

    close(listen_sock);

    return 0;
}