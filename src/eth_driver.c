#include <stdio.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <stdlib.h>

#include "eth_driver.h"
#include "eth_driver_local.h"

char mac_address[6];
int packet_read_pointer;
int tx_queue_usage;
int tx_length0;
int tx_length1;
int tx_pending_flag;

int test_present() {
	clear_control_register(ECON1, 0x3);
	int read = read_control_register(ECON1);
	if ((read & 0x3) != 0) {
		return 0;
	}
	set_control_register(ECON1, 0x3);
	read = read_control_register(ECON1);
	if ((read & 0x3) != 0x3) {
		return 0;
	}
	set_bank(0);
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
	for (j = 0; j < sizeof(mac_address); j++) {
		mac_address[j] = rand();
	}

	mac_address[0] &= 0xFE; // Unicast address
	mac_address[0] |= 0x02; // Local MAC address (not globally unique)

	set_bank(0);
	set_rx_range(TX_SIZE, MEM_HIGH);
	set_write_pointer(0x0000);
	set_read_pointer(TX_SIZE);

	tx_queue_usage = 0;
	tx_pending_flag = 0;

	int clk_ready = 0;
	while (!clk_ready) {
		clk_ready =  read_control_register(ESTAT) & 0x01;
	}

	set_receive_enable(1);

	set_bank(1);
	int last = 0;

	char send_buf[120];
	memset(send_buf, 0xFF, 6);
	memcpy(send_buf + 6, mac_address, 6);
	memset(send_buf + 12, 0x55, 108);

	char packet_buffer[8192];
	while (1) {
		//usleep(20);
		/*set_bank(0);
		if (queue_packet(send_buf, 120)) {
			printf("Queuing \n");
			int i;
			for (i = 0; i < 120; i++) {
				printf("%02x", send_buf[i]);
			}
			printf("\n");
		}
		if (poll_tx_send()) {
			printf("Succcessful polled\n");
		}*/
		int len = get_packet(packet_buffer, sizeof(packet_buffer));
		if (len > 0) {
			printf("Received ");
			int i;
			for (i = 0; i < len; i++) {
				printf("%02x", packet_buffer[i]);
			}
			printf("\n");
		}
	}

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

int get_packet(char* buffer, int len) {
	set_bank(1);
	int count = read_control_register(EPKTCNT);
	set_bank(0);
	if (count == 0) {
		return -1;
	}
	int next_packet = read_buffer_short();
	int size = next_packet - packet_read_pointer - 6;
	if (size < 0) {
		size += MEM_HIGH + 1 - TX_SIZE;
	}
	int status = read_buffer_int();
	if (size <= len) {
		buffer_read(buffer, 0, size);
	}
	packet_read_pointer = next_packet;
	set_bank(0);
	write_control_register(ERXRDPTL, next_packet);
	write_control_register(ERXRDPTH, next_packet >> 8);
	set_control_register(ECON2, ECON2_PKTDEC);
	if (size > len) {
		fprintf(stderr, "Packet exceeded maximum size\n");
                return -1;
	}
	return size;
}

int queue_packet(char *buffer, int len) {
	if (tx_queue_usage == 0x03) {
		return 0;
	}
	int tx_bank;
	int start;
	int *length_ptr;
	if (tx_queue_usage & 0x01) {
		tx_bank = 0x02;
		start = TX_SIZE >> 1;
		length_ptr = &tx_length1;
	} else {
		tx_bank = 0x01;
		start = 0;
		length_ptr = &tx_length0;
	}
	set_write_pointer(start);
	char control_prefix[1];
	control_prefix[0] = 0x0E;
	buffer_write(control_prefix, 0, 1);
	buffer_write(buffer, 0, len);
	*length_ptr = len;
	tx_queue_usage |= tx_bank;
	return 1;
}

int poll_tx_send() {
	if (tx_queue_usage == 0) {
		return 0;
	}
	set_bank(0);
	if (tx_pending_flag != 0) {
		int tx_rts = read_control_register(ECON1) & ECON1_TXRTS;
		if (tx_rts) {
			return 0;
		}
		tx_queue_usage &= ~tx_pending_flag;
		tx_pending_flag = 0;
	}
	if (tx_queue_usage == 0) {
		return 0;
	}
	int tx_bank;
	int start;
	int *length_ptr;
	if (!(tx_queue_usage & 0x01)) {
                tx_bank = 0x02;
                start = TX_SIZE >> 1;
                length_ptr = &tx_length1;
        } else {
                tx_bank = 0x01;
                start = 0;
                length_ptr = &tx_length0;
        }
	write_register_word(ETXSTL, start);
	write_register_word(ETXNDL, start + (*length_ptr));
	tx_pending_flag |= tx_bank;
	set_control_register(ECON1, ECON1_TXRTS);
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

void set_bank(int bank) {
	clear_control_register(ECON1, ECON1_BSEL);
	set_control_register(ECON1, bank & ECON1_BSEL);
}

void write_register_word(int address, int data) {
	write_control_register(address, data);
	write_control_register(address + 1, data >> 8);
}

int read_register_word(int address) {
	int data = read_control_register(address + 1) << 8;
	data |= read_control_register(address) & 0xFF;
}

void set_read_pointer(int address) {
	write_register_word(ERDPTL, address);
	packet_read_pointer = address;
}

void set_write_pointer(int address) {
	write_register_word(EWRPTL, address);
}

void set_rx_range(int start, int end) {
	write_register_word(ERXSTL, start);
	write_register_word(ERXNDL, end);
}

void wait_busy() {
	usleep(12);
	set_bank(3);
	while (read_control_register(MISTAT) & MISTAT_BUSY) {
		usleep(1);
	}
	set_bank(2);
}

int read_phy_reg(int address) {
	set_bank(2);
	write_control_register(MIREGADR, address & 0x1F);
	set_control_register(MICMD, MICMD_MIIRD);
	wait_busy();
	clear_control_register(MICMD, MICMD_MIIRD);
	int read = read_control_register(MIRDH);
	read <<= 8;
	read |= read_control_register(MIRDL);
}

void write_phy_reg(int address, int data) {
	set_bank(2);
	write_control_register(MIREGADR, address & 0x1F);
	write_control_register(MIWRL, data);
	write_control_register(MIWRH, data >> 8);
	wait_busy();
}

void set_receive_enable(int receive) {
	if (receive) {
		clear_control_register(ECON1, ECON1_RXEN);
		set_bank(2);
		write_control_register(MACON1, MACON1_TXPAUS | MACON1_RXPAUS | MACON1_PASSALL | MACON1_MARXEN);
		write_control_register(MACON3, MACON3_PADCFG | MACON3_TXCRCEN | MACON3_FRMLNEN | MACON3_FULDPX);
		write_control_register(MACON4, MACON4_DEFER);
		write_register_word(MAMXFLL, 1518);
		write_control_register(MABBIPG,  0x15);
		write_register_word(MAIPGL, 0x12);

		set_bank(3);
		write_control_register(MAADR1, mac_address[0]);
		write_control_register(MAADR2, mac_address[1]);
		write_control_register(MAADR3, mac_address[2]);
		write_control_register(MAADR4, mac_address[3]);
		write_control_register(MAADR5, mac_address[4]);
		write_control_register(MAADR6, mac_address[5]);

		write_phy_reg(PHCON1, PHCON1_PDPXMD);

		set_bank(0);
		set_control_register(ECON1, ECON1_RXEN);
	} else {
		set_bank(2);
		clear_control_register(MACON1, MACON1_MARXEN);
		set_bank(0);
		clear_control_register(ECON1, ECON1_RXEN);
	}
}
