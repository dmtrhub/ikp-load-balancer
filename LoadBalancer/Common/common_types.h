#ifndef COMMON_TYPES_H
#define COMMON_TYPES_H

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

#define _WINSOCKAPI_  // Prevent inclusion of winsock.h

#include <windows.h>
#include <winsock2.h>
#include <ws2tcpip.h>

#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <stdbool.h>

#pragma comment(lib, "Ws2_32.lib")

// Energy consumption request
typedef struct {
    int userId;
    int socketId;
    float consumedEnergy;  // kW
} EnergyRequest;

// Price calculation response
typedef struct {
    int userId;
    int socketId;
    float greenEnergy;     // kWh in green zone
    float blueEnergy;      // kWh in blue zone
    float redEnergy;       // kWh in red zone
    float totalCost;       // RSD
} PriceResult;

// Price configuration sent to workers
typedef struct {
    float greenBorder;     // kWh threshold for green
    float blueBorder;      // kWh threshold for blue
    float greenPrice;      // RSD/kWh
    float bluePrice;       // RSD/kWh
    float redPrice;        // RSD/kWh
} PricingConfiguration;

#endif
