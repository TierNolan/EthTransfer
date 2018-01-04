#ifndef IP_STACK_LOCAL_H
#define IP_STACK_LOCAL_H

#include <stdint.h>
#include <arpa/inet.h>

#include "eth_driver.h"

#define TCP_WINDOW (0x1000)

#define RX_MASK (0x1FFF)
#define TX_MASK (RX_MASK)
#define BUFFER_SIZE (0x1000)
#define MAX_SEGMENT (1400)
#define SEG_MASK (0x1F)

#define IP_NULL (0)
#define IP_PING (1)
#define IP_TCP (2)

#define TCP_LISTEN (0)
#define TCP_SYN_RECEIVED (1)
#define TCP_SYN_ACK_SENT (2)
#define TCP_CONNECTED (3)
#define TCP_RX_CLOSED (4)
#define TCP_TX_CLOSED (5)
#define TCP_BOTH_CLOSED (6)
#define TCP_SEND_RST (7)
#define TCP_DATA_TO_READ (8)

#define TCP_ACK (0x10)
#define TCP_RST (0x04)
#define TCP_SYN (0x02)
#define TCP_FIN (0x01)

#define ACK_TIMEOUT (20)

typedef struct _tcp_segment {
        uint32_t position;
        uint32_t length;
} tcp_segment;

typedef struct _tcp_conection {
	int connection_id;
        uint8_t rx_buffer[RX_MASK + 1];
        uint8_t tx_buffer[TX_MASK + 1];
        int state;
        uint16_t local_port;
        uint32_t local_sequence;
	eth_mac remote_mac;
        uint16_t remote_port;
        uint32_t remote_ip;
        uint32_t remote_sequence;
	int remote_window_scale;
        uint32_t remote_window;
	uint32_t remote_max_segment;
	uint32_t rx_write;
	uint32_t rx_read;
        uint32_t rx_acked;
	uint32_t rx_window;
	uint32_t rx_fin;
	uint32_t tx_write;
	uint32_t tx_sent;
	uint32_t tx_acked;
	uint32_t tx_limit;
	uint32_t tx_fin;
	long ack_timeout;
        tcp_segment rx_segments[SEG_MASK + 1];
	int segment_count;
} tcp_connection;

#pragma pack(push, 1)

typedef struct _arp_header {
	uint16_t htype;
	uint16_t ptype;
	uint8_t hlen;
	uint8_t plen;
	uint16_t oper;
	eth_mac sha;
	uint32_t spa;
	eth_mac tha;
	uint32_t tpa;
} arp_header;

typedef struct _ip_header {
	uint8_t version_ihl;
	uint8_t dscp_ecn;
	uint16_t total_length;
	uint16_t identification;
	uint16_t fragment_offset;
	uint8_t ttl;
	uint8_t protocol;
	uint16_t checksum;
	uint32_t source_ip;
	uint32_t dest_ip;
} ip_header;

typedef struct _ping_header {
	uint8_t type;
	uint8_t code;
	uint16_t checksum;
	uint16_t identifier;
	uint16_t sequence_number;
	uint8_t payload[];
} ping_header;

typedef struct _tcp_header {
	uint16_t source_port;
	uint16_t dest_port;
	uint32_t sequence;
	uint32_t ack;
	uint16_t offset_flags;
	uint16_t window_size;
	uint16_t checksum;
	uint16_t urgent;
	uint8_t options[];
} tcp_header;

#pragma pack(pop)

typedef struct _arp_packet {
	eth_packet* eth_packet;
	arp_header* arp_header;
	int length;
} arp_packet;

typedef struct _ip_packet {
	eth_packet* eth_packet;
	ip_header* ip_header;
	ping_header* ping_header;
	tcp_header* tcp_header;
	int length;
} ip_packet;

uint16_t ip_checksum(uint8_t* buf, int len);
uint32_t ip_checksum_init();
uint32_t ip_checksum_add(uint32_t checksum, uint8_t* buf, int len);
uint32_t ip_checksum_final(uint32_t checksum);

int handle_rx_packet();

void new_arp_packet(arp_packet* packet, eth_packet* eth_packet, int validate);
void handle_arp_packet(arp_packet arp_packet);
void ntoh_arp(arp_packet* packet);
void hton_arp(arp_packet* packet);

void new_ip_packet(ip_packet* packet, eth_packet* eth_packet, int validate);
void handle_ip_packet(ip_packet ip_packet);
void ntoh_ip(ip_packet* packet);
void hton_ip(ip_packet* packet);

void handle_ping_packet(ip_packet rx_ip_packet);
void ntoh_ping(ping_header* header);
void hton_ping(ping_header* header);

void handle_tcp_packet(ip_packet rx_ip_packet);
void ntoh_tcp(tcp_header* header);
void hton_tcp(tcp_header* header);
int send_tcp_packet();

int add_segment(uint8_t* data, int data_length, uint32_t dest);

long get_now();
int check_timeout(long timeout);

int scan_options(uint8_t* buf, int len, int kind);

uint16_t ip_checksum(uint8_t* buf, int len);
uint32_t ip_checksum_init();
uint32_t ip_checksum_add(uint32_t checksum, uint8_t* buf, int len);
uint32_t ip_checksum_final(uint32_t checksum);

void copy_to_circle_buffer(uint8_t* dest, int dest_mask, int dest_start, uint8_t* source, int source_size, int length);
void copy_from_circle_buffer(uint8_t* dest, int dest_size, uint8_t* source, int source_mask, int source_start, int length);

void send_broadcast_arp();

/*
int create_tcp_packet(int8_t* send_buffer, int buf_len, eth_mac mac_dest, eth_mac mac_src, uint32_t dest_ip, uint16_t source_port, uint16_t dest_port, uint32_t sequence, uint32_t ack, uint16_t flags, int8_t* data, int data_len);
void send_tcp_tx_buffer();

void handle_ip_packet(eth_packet* rx_packet, ip_packet* rx_ip_packet, int len);
void handle_ping_packet(eth_packet* rx_packet, ip_packet* rx_ip_packet, int len);
void handle_tcp_packet(eth_packet* rx_packet, ip_packet* rx_ip_packet, int len);

void handle_arp_packet(eth_packet* rx_packet, arp_packet* rx_arp_packet);

void ntoh_ip(ip_packet* packet);
void hton_ip(ip_packet* packet);

void ntoh_ping(ping_packet* packet);
void hton_ping(ping_packet* packet);

void ntoh_tcp(tcp_packet* packet);
void hton_tcp(tcp_packet* packet);
*/

#endif
