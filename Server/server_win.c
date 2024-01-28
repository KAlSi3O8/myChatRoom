#define WIN32_LEAN_AND_MEAN

#include <windows.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <pthread.h>
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>

#define     PORT        "6010"
#define     MAXCLIENTS  5
#define     BUFLEN      1500

struct s_client_info {
    pthread_t tid;
    SOCKET socket;
} clients[MAXCLIENTS];

void *service(void *arg) {
    struct s_client_info *p_clients = (struct s_client_info *)arg;
    char buf[BUFLEN];
    int len;
    int ret;

    printf("client service Entry--> [%d]%d\n", p_clients->tid, p_clients->socket);

    memset(buf, 0, BUFLEN);

    while (1) {
        len = recv(p_clients->socket, buf, BUFLEN, 0);
        printf("[%d]%d: %s\n", p_clients->tid, p_clients->socket, buf);
        if (len > 0) {
            for (int i = 0; i < MAXCLIENTS; i++) {
                // boardcast ot other clients
                if (clients[i].socket != p_clients->socket && clients[i].socket != 0) {
                    eagain:
                    ret = send(clients[i].socket, buf, len, 0);
                    if (ret == SOCKET_ERROR) {
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
            closesocket(p_clients->socket);
            p_clients->socket = 0;
            return NULL;
        }
    }
}

int main(int argc, char *argv[]) {
    int ret;

    SOCKET listen_sock = INVALID_SOCKET;
    SOCKET connect_sock = INVALID_SOCKET;

    struct addrinfo *result = NULL;
    struct addrinfo hints;

    pthread_attr_t attr;

    ret = pthread_attr_init(&attr);
    if (ret != 0) {
        perror("pthread_attr_init");
    }

    WSADATA wsaData;
    ret = WSAStartup(MAKEWORD(2,2), &wsaData);
    if (ret != 0) {
        perror("WSAStartup");
        return 1;
    }

    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP;
    hints.ai_flags = AI_PASSIVE;

    ret = getaddrinfo(NULL, PORT, &hints, &result);
    if (ret != 0) {
        perror("getaddrinfo");
        WSACleanup();
        return 1;
    }

    for (struct addrinfo *cur = result; cur != NULL; cur = cur->ai_next) {
        char ipbuf[16];
        unsigned short port;
        struct sockaddr_in *addr = (struct sockaddr_in *)cur->ai_addr;
        inet_ntop(AF_INET, &addr->sin_addr, ipbuf, 16);
        port = ntohs(addr->sin_port);
        printf("ip: %s, port: %d\n", ipbuf, port);
    }

    listen_sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (listen_sock == INVALID_SOCKET) {
        freeaddrinfo(result);
        WSACleanup();
        perror("socket");
    }

    ret = bind( listen_sock, result->ai_addr, (int)result->ai_addrlen);
    if (ret == SOCKET_ERROR)  {
        perror("bind");
        freeaddrinfo(result);
        closesocket(listen_sock);
        WSACleanup();
    }

    freeaddrinfo(result);

    ret = listen(listen_sock, SOMAXCONN);
    if (ret == SOCKET_ERROR) {
        perror("listen");
        closesocket(listen_sock);
        WSACleanup();
        return 1;
    }

    printf("wait for clients\n");

    while (1) {
        loop_start:
        connect_sock = accept(listen_sock, NULL, NULL);
        if (connect_sock == INVALID_SOCKET) {
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

        closesocket(connect_sock);
    }

    for (int i = 0; i < MAXCLIENTS; i++) {
        pthread_join(clients[i].tid, NULL);
    }

    closesocket(listen_sock);
    WSACleanup();

    return 0;
}