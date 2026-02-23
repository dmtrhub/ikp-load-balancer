#ifndef NETWORK_UTILS_H
#define NETWORK_UTILS_H

#include "common_types.h"

int init_networks();
void cleanup_networks(SOCKET* sockets, int count);

SOCKET setup_server_socket(int port);

int send_all(SOCKET sock, void* buffer, int length);
int recv_all(SOCKET sock, void* buffer, int length);

#endif