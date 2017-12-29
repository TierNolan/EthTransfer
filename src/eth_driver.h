#ifndef ETH_DRIVER_H
#define ETH_DRIVER_H

#include <stdint.h>
#include "gpio_spi.h"

#define PACKET_MAX_SIZE (2048)

#define ETH_NULL (0)
#define ETH_ARP_PACKET (1)
#define ETH_IP_PACKET (2)

#define ETH_UNKNOWN (255)

#pragma pack(push, 1)

typedef struct _mac {
	char mac[6];
} eth_mac;

typedef struct _eth_header {
	eth_mac dest;
	eth_mac source;
	int16_t ether_type;
} eth_header;

#pragma pack(pop)

typedef struct _eth_packet {
	uint8_t buffer[PACKET_MAX_SIZE];
	int length;
	int type;
	eth_header* header;
	uint8_t* data;
} eth_packet;

extern eth_mac ETH_BROADCAST_ADDRESS;
extern eth_mac ETH_NULL_ADDRESS;

int init_ethernet();
int close_ethernet();

void new_eth_packet(eth_packet* packet);

int get_packet(eth_packet* eth_packet);
int send_packet(eth_packet* packet);

eth_mac get_local_mac();

#endif
