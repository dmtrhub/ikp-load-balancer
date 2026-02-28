#ifndef NETWORK_UTILS_H
#define NETWORK_UTILS_H

#include "common_types.h"

// Network initialization
int init_networks();
void cleanup_networks(SOCKET* socketsToClose, int count);

// Send/receive operations
int send_all(SOCKET s, void* data, int size);
int recv_all(SOCKET s, void* data, int size);

// Server setup
SOCKET connect_to_server(const char* ip, int port);
SOCKET setup_server_socket(int port);

#endif
