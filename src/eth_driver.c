#include <stdio.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <stdlib.h>
#include <arpa/inet.h>

#include "eth_driver.h"
#include "eth_driver_local.h"

eth_mac mac_address;

int test_present() {
	push_bank(0);
	clear_control_register(ECON1, 0x3);
	int read = read_control_register(ECON1);
	pop_bank();

	if ((read & 0x3) != 0) {
		pop_bank();
		return 0;
	}

	push_bank(0);
	set_control_register(ECON1, 0x3);
	read = read_control_register(ECON1);
	pop_bank();

	if ((read & 0x3) != 0x3) {
		return 0;
	}
	return 1;
}

int init_ethernet() {
	if (!init_gpio_spi()) {
		fprintf(stderr, "Unable to initialize spi gpio driver\n");
		return 0;
	}

	soft_reset();

	if (!test_present()) {
		fprintf(stderr, "Unable to detect ENC28J60 board\n");
		return 0;
	}

        srand(time(NULL));
        int j;
        for (j = 0; j < sizeof(mac_address.mac); j++) {
                mac_address.mac[j] = rand();
        }
	mac_address.mac[0] &= 0xFE; // Unicast address
        mac_address.mac[0] |= 0x02; // Local MAC address (not globally unique)

	// NOTE: read range must start at location 0 due to HW bug
	// See:  ENC28J60 Rev. B7 Silicon Errata   (Section 3)
	set_rx_range(0, RX_END);
	set_write_pointer(TX_START);
	set_read_pointer(0);

	int clk_ready = 0;

	int i;
	for (i = 0; i < 100 && !clk_ready; i++) {
		clk_ready =  read_control_register(ESTAT) & 0x01;
		usleep(10);
	}

	if (!clk_ready) {
		fprintf(stderr, "ENC28J60 clock not stable after timeout\n");
		return 0;
	}

	set_receive_enable(1);

	return 1;
}

int read_buffer_short() {
	char buf[2];
	buffer_read(buf, 0, 2);
	return (buf[1] << 8) | buf[0];
}

int read_buffer_int() {
	char buf[4];
	buffer_read(buf, 0, 4);
	return (buf[3] << 24) | (buf[2] << 16) | (buf[1] << 8) | buf[0];
}

int get_packet(eth_packet* packet) {
	push_bank(1);
	int count = read_control_register(EPKTCNT);
	pop_bank();

	if (count == 0) {
		packet->type = ETH_NULL;
		return 0;
	}

	push_bank(0);
        int read_pointer = read_control_register_word(ERDPTL);
	pop_bank();

	int next_packet = read_buffer_short();

	int size = next_packet - read_pointer - 6;
	if (size < 0) {
		size += RX_END + 1;
	}

	int status = read_buffer_int();
	if (size <= PACKET_MAX_SIZE) {
		buffer_read(packet->buffer, 0, size);
	} else {
		write_control_register_word(ERDPTL, next_packet);
	}

	push_bank(0);
	write_control_register_word(ERXRDPTL, next_packet);
	pop_bank();

	set_control_register(ECON2, ECON2_PKTDEC);
	if (size > PACKET_MAX_SIZE) {
		fprintf(stderr, "Packet exceeded maximum size %04x > %04x\n", size, PACKET_MAX_SIZE);
		packet->type = ETH_NULL;
                return 0;
	}

	packet->length = size - 4;
	packet->header = (eth_header*) packet;

	if (size < 14) {
		packet->type = ETH_UNKNOWN;
	} else {
		ntoh_eth(packet);
		int ether_type = packet->header->ether_type;

		if (ether_type == 0x0800) {
			packet->type = ETH_IP_PACKET;
		} else if (ether_type == 0x0806) {
			packet->type = ETH_ARP_PACKET;
		} else {
			packet->type = ETH_UNKNOWN;
		}
	}
	return 1;
}

int send_packet(eth_packet* packet) {
	if (packet->type == ETH_NULL) {
		return 0;
	}
	int length = packet->length;
	if (length < 0 || length > PACKET_MAX_SIZE) {
		printf("Packet size out of range\n");
		return 0;
	}

	hton_eth(packet);

	set_write_pointer(0);
	char control_prefix[1];
	control_prefix[0] = 0x0E;
	buffer_write(control_prefix, 0, 1);
	buffer_write(packet->buffer, 0, length);

	ntoh_eth(packet);

	push_bank(0);
	write_control_register_word(ETXSTL, 0);
	write_control_register_word(ETXNDL, length);
	pop_bank();

	set_control_register(ECON1, ECON1_TXRTS);

	usleep(1);

	while (read_control_register(ECON1) & ECON1_TXRTS)
		;

	return 1;
}

int close_ethernet() {
	set_receive_enable(0);
	if (!close_gpio_spi()) {
		fprintf(stderr, "Unable to close spi gpio driver\n");
		return 0;
	}
	return 1;
}

void new_eth_packet(eth_packet* packet) {
	packet->length = -1;
	packet->type = ETH_NULL;
	packet->header = (eth_header*) packet->buffer;
	packet->data = ((uint8_t*) packet->buffer) + 14;
}

void set_rx_range(int start, int end) {
	push_bank(0);
	write_control_register_word(ERXSTL, start);
	write_control_register_word(ERXNDL, end);
	pop_bank();
}

void set_receive_enable(int receive) {
	if (receive) {
		clear_control_register(ECON1, ECON1_RXEN);
		push_bank(2);
		write_control_register(MACON1, MACON1_TXPAUS | MACON1_RXPAUS | MACON1_PASSALL | MACON1_MARXEN);
		write_control_register(MACON3, MACON3_PADCFG | MACON3_TXCRCEN | MACON3_FRMLNEN | MACON3_FULDPX);
		write_control_register(MACON4, MACON4_DEFER);
		write_control_register_word(MAMXFLL, 1518);
		write_control_register(MABBIPG,  0x15);
		write_control_register_word(MAIPGL, 0x12);
		pop_bank();

		push_bank(3);
		write_control_register(MAADR1, mac_address.mac[0]);
		write_control_register(MAADR2, mac_address.mac[1]);
		write_control_register(MAADR3, mac_address.mac[2]);
		write_control_register(MAADR4, mac_address.mac[3]);
		write_control_register(MAADR5, mac_address.mac[4]);
		write_control_register(MAADR6, mac_address.mac[5]);
		pop_bank();

		write_phy_reg(PHCON1, PHCON1_PDPXMD);

		set_control_register(ECON1, ECON1_RXEN);
	} else {
		push_bank(2);
		clear_control_register(MACON1, MACON1_MARXEN);
		pop_bank();

		clear_control_register(ECON1, ECON1_RXEN);
	}
}

void ntoh_eth(eth_packet* packet) {
	packet->header->ether_type = ntohs(packet->header->ether_type);
}

void hton_eth(eth_packet* packet) {
       	packet->header->ether_type = htons(packet->header->ether_type);
}

eth_mac get_local_mac() {
	return mac_address;
}
