#include "network_utils.h"

int init_networks() {
	WSADATA wsa;
	return WSAStartup(MAKEWORD(2, 2), &wsa) == 0;
}

void cleanup_networks(SOCKET* socketsToClose, int count) {
	for (int i = 0; i < count; i++) {
		if (socketsToClose[i] != INVALID_SOCKET) {
			closesocket(socketsToClose[i]);
		}
	}

	WSACleanup();
}

int send_all(SOCKET s, void* data, int size) {
	char* ptr = (char*)data;
	while (size > 0) {
		int sent = send(s, ptr, size, 0);
		if (sent <= 0) {
			return 0;
		}
		ptr += sent;
		size -= sent;
	}
	return 1;
}

int recv_all(SOCKET s, void* data, int size) {
	char* ptr = (char*)data;
	while (size > 0) {
		int received = recv(s, ptr, size, 0);
		if (received <= 0) {
			return 0;
		}
		ptr += received;
		size -= received;
	}
	return 1;
}

SOCKET connect_to_server(const char* ip, int port) {
	SOCKET s = socket(AF_INET, SOCK_STREAM, 0);
	if (s == INVALID_SOCKET) {
		return INVALID_SOCKET;
	}

	struct sockaddr_in server;
	server.sin_family = AF_INET;
	server.sin_port = htons(port);

	if (inet_pton(AF_INET, ip, &server.sin_addr) <= 0) {
		closesocket(s);
		return INVALID_SOCKET;
	}

	if (connect(s, (struct sockaddr*)&server, sizeof(server)) == SOCKET_ERROR) {
		closesocket(s);
		return INVALID_SOCKET;
	}
	return s;
}

SOCKET setup_server_socket(int port) {
	SOCKET listenSock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (listenSock == INVALID_SOCKET) {
		return INVALID_SOCKET;
	}

	char reuse = 1;
	setsockopt(listenSock, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));

	struct sockaddr_in addr;
	addr.sin_family = AF_INET;
	addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
	addr.sin_port = htons(port);

	if (bind(listenSock, (struct sockaddr*)&addr, sizeof(addr)) == SOCKET_ERROR) {
		closesocket(listenSock);
		return INVALID_SOCKET;
	}

	if (listen(listenSock, SOMAXCONN) == SOCKET_ERROR) {
		closesocket(listenSock);
		return INVALID_SOCKET;
	}

	return listenSock;
}