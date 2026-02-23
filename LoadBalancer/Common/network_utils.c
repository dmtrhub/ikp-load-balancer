#include "network_utils.h"

int init_networks() {
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        printf("[NETWORK] WSAStartup failed: %d\n", WSAGetLastError());
        return 0;
    }
    printf("[NETWORK] WinSock initialized\n");
    return 1;
}

void cleanup_networks(SOCKET* sockets, int count) {
    for (int i = 0; i < count; i++) {
        if (sockets[i] != INVALID_SOCKET) {
            closesocket(sockets[i]);
        }
    }
    WSACleanup();
    printf("[NETWORK] WinSock cleanup complete\n");
}

SOCKET setup_server_socket(int port) {
    SOCKET listenSock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (listenSock == INVALID_SOCKET) {
        printf("[NETWORK] Socket creation failed\n");
        return INVALID_SOCKET;
    }

    struct sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port = htons(port);

    if (bind(listenSock, (struct sockaddr*)&addr, sizeof(addr)) == SOCKET_ERROR) {
        printf("[NETWORK] Bind failed on port %d\n", port);
        closesocket(listenSock);
        return INVALID_SOCKET;
    }

    if (listen(listenSock, SOMAXCONN) == SOCKET_ERROR) {
        printf("[NETWORK] Listen failed\n");
        closesocket(listenSock);
        return INVALID_SOCKET;
    }

    return listenSock;
}

int send_all(SOCKET sock, void* buffer, int length) {
    int sent = 0;
    while (sent < length) {
        int result = send(sock, (char*)buffer + sent, length - sent, 0);
        if (result == SOCKET_ERROR) {
            printf("[NETWORK] Send failed: %d\n", WSAGetLastError());
            return 0;
        }
        sent += result;
    }
    return 1;
}

int recv_all(SOCKET sock, void* buffer, int length) {
    int received = 0;
    while (received < length) {
        int result = recv(sock, (char*)buffer + received, length - received, 0);
        if (result == SOCKET_ERROR || result == 0) {
            printf("[NETWORK] Recv failed: %d\n", WSAGetLastError());
            return 0;
        }
        received += result;
    }
    return 1;
}