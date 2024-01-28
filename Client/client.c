#define WIN32_LEAN_AND_MEAN

#include <windows.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <pthread.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#define     PORT        "6010"
#define     BUFLEN      1500
#define     NAMELEN     20
#define     err       WSAGetLastError()

char gets_buf[BUFLEN] = {0};

struct s_pdata {
    char *name;
    char *msg;
};

void *send_thread (void *arg) {
    SOCKET *socket = (SOCKET *)arg;
    struct s_pdata pdata;
    char buf[BUFLEN];
    int size;
    int ret;

    memset(buf, 0, BUFLEN);
    usleep(20000);

    pdata.name = buf;
    printf("what's your name: ");
    if (NULL != fgets(pdata.name, NAMELEN, stdin)) {
        size = strlen(pdata.name) - 1;
        ret = sprintf(buf + size, " Enter the room.\n");
        ret = send(*socket, buf, size + ret, 0);
        if (ret == -1) {
            printf("send error!\n");
        }
    } else {
        return NULL;
    }

    pdata.name[size++] = ':';
    pdata.name[size++] = ' ';
    pdata.msg = buf + size;

    while(1) {
        if (NULL != fgets(pdata.msg, BUFLEN, stdin)) {
            ret = send(*socket, buf, strlen(buf), 0);
            if (ret == SOCKET_ERROR) {
                printf("%s ret: %d, err: %d\n",__FUNCTION__ , ret, err);
                if (err == WSAECONNRESET) {
                    break;
                }
            }
        }
    }

    printf("send_thread Exit-->\n");
    return NULL;
}

void *recv_thread (void *arg) {
    SOCKET *socket = (SOCKET *)arg;
    char buf[BUFLEN];
    int len;

    memset(buf, 0, BUFLEN);

    while(1) {
        len = recv(*socket, buf, BUFLEN, 0);
        buf[len] = 0;
        if (len > 0) {
            printf("\r%s", buf);
        } else {
            printf("%s len: %d, err: %d\n",__FUNCTION__ , len, err);
            break;
        }
    }

    printf("recv_thread Exit-->\n");
    return NULL;
}

int main(int argc, char *argv[]) {
    int ret;

    SOCKET client_sock = INVALID_SOCKET;

    struct addrinfo *result = NULL;
    struct addrinfo hints;

    pthread_t tid_send, tid_recv;
    pthread_attr_t attr;

    ret = pthread_attr_init(&attr);
    if (ret != 0) {
        printf("%d: %d\n", __LINE__, err);
    }

    WSADATA wsaData;
    ret = WSAStartup(MAKEWORD(2,2), &wsaData);
    if (ret != 0) {
        printf("%d: %d\n", __LINE__, err);
        return 1;
    }

    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP;
    hints.ai_flags = AI_PASSIVE;

    if (argc < 2) {
        ret = getaddrinfo("www.tokiinu.top", PORT, &hints, &result);
    } else {
        ret = getaddrinfo(argv[1], PORT, &hints, &result);
    }

    ret = getaddrinfo(argv[1], PORT, &hints, &result);
    if (ret != 0) {
        printf("%d: %d\n", __LINE__, err);
        WSACleanup();
        return 1;
    }

    reconnect:
    client_sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (client_sock == INVALID_SOCKET) {
        printf("%d: %d\n", __LINE__, err);
        freeaddrinfo(result);
        WSACleanup();
        return 1;
    }

    ret = connect(client_sock, result->ai_addr, result->ai_addrlen);
    if (ret == SOCKET_ERROR) {
        printf("ret: %d, err: %d\n", ret, err);
        closesocket(client_sock);
        sleep(5);
        goto reconnect;
    }

    freeaddrinfo(result);

    pthread_create(&tid_recv, &attr, &recv_thread, &client_sock);
    pthread_create(&tid_send, &attr, &send_thread, &client_sock);

    pthread_join(tid_recv, NULL);
    pthread_join(tid_send, NULL);

    closesocket(client_sock);

    return 0;
}