#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>

#include "ip_stack.h"
#include "ip_stack_local.h"
#include "eth_driver.h"

eth_mac ETH_BROADCAST_ADDRESS = {{0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF}};
eth_mac ETH_NULL_ADDRESS = {{0x00, 0x00, 0x00, 0x00, 0x00, 0x00}};

long base_time;
int base_time_set = 0;

int ip_addr;
uint16_t identification_counter;
int connection_counter;
tcp_connection tcp_conn;

int init_ip(uint32_t addr) {
	if (!init_ethernet()) {
		return 0;
	}
	ip_addr = addr;
	identification_counter = rand();
	connection_counter = 1;
	tcp_conn.state = TCP_LISTEN;
	return 1;
}

int close_ip() {
	connection_counter = -1;
	tcp_conn.state = TCP_LISTEN;
	return close_ethernet();
}

void ip_stack_poll() {
	int action_taken = 0;

	int i = 10;
	while (handle_rx_packet() && (0 <= i--)) {
		action_taken = 1;
	}

	i = 10;
	while (send_tcp_packet() && (0 <= i--)) {
		action_taken = 1;
	}

	if (!action_taken) {
		usleep(500);
	}
}

int handle_rx_packet() {
	eth_packet rx_eth_packet_inst;
	new_eth_packet(&rx_eth_packet_inst);

	eth_packet* rx_eth_packet = &rx_eth_packet_inst;

	if (get_packet(rx_eth_packet)) {
		if (rx_eth_packet->type == ETH_ARP_PACKET) {
			arp_packet rx_arp_packet; 
			new_arp_packet(&rx_arp_packet, rx_eth_packet, 1);

			handle_arp_packet(rx_arp_packet);
		} else if (rx_eth_packet->type == ETH_IP_PACKET) {
			ip_packet rx_ip_packet;
			new_ip_packet(&rx_ip_packet, rx_eth_packet, 1);

			handle_ip_packet(rx_ip_packet);
		}
		return 1;
	}
	return 0;
}

// ARP Functions //

void new_arp_packet(arp_packet* arp_packet, eth_packet* eth_packet, int validate) {
	arp_packet->eth_packet = eth_packet;
	arp_packet->arp_header = (arp_header*) (eth_packet->data);
	arp_packet->length = 28;

        ntoh_arp(arp_packet);

	if (validate) {
		int valid = 1;
		valid &= arp_packet->eth_packet->length >= 42;
		valid &= arp_packet->arp_header->htype == 1;
		valid &= arp_packet->arp_header->ptype == 0x0800;
		valid &= arp_packet->arp_header->hlen == 0x06;
		valid &= arp_packet->arp_header->plen == 0x04;
		valid &= arp_packet->arp_header->oper == 0x01 || arp_packet->arp_header->oper == 0x02;

		if (!valid) {
			arp_packet->length = -1;
			return;
		}
	} else {
		arp_packet->eth_packet->length = 42;
		arp_packet->arp_header->htype = 1;
		arp_packet->arp_header->ptype = 0x0800;
		arp_packet->arp_header->hlen = 0x06;
		arp_packet->arp_header->plen = 0x04;
	}
}

void handle_arp_packet(arp_packet rx_arp_packet) {
	if (rx_arp_packet.length < 0) {
		return;
	}

        int oper = rx_arp_packet.arp_header->oper;

        if (oper == 0x0001) {
                if (rx_arp_packet.arp_header->tpa == ip_addr) {
                        eth_packet tx_packet;
			new_eth_packet(&tx_packet);

			tx_packet.type = ETH_ARP_PACKET;

			arp_packet tx_arp_packet;
			new_arp_packet(&tx_arp_packet, &tx_packet, 0);

			tx_arp_packet.eth_packet->header->dest = rx_arp_packet.eth_packet->header->source;
			tx_arp_packet.eth_packet->header->source = get_local_mac();
			tx_arp_packet.eth_packet->header->ether_type = rx_arp_packet.eth_packet->header->ether_type;

                        tx_arp_packet.arp_header->oper = 2;
                        tx_arp_packet.arp_header->sha = get_local_mac();
                        tx_arp_packet.arp_header->spa = ip_addr;
                        tx_arp_packet.arp_header->tha = rx_arp_packet.arp_header->sha;
                        tx_arp_packet.arp_header->tpa = rx_arp_packet.arp_header->spa;

                        hton_arp(&tx_arp_packet);

                        send_packet(tx_arp_packet.eth_packet);
                }
        }
}

void ntoh_arp(arp_packet* packet) {
        packet->arp_header->htype = ntohs(packet->arp_header->htype);
        packet->arp_header->ptype = ntohs(packet->arp_header->ptype);
        packet->arp_header->oper = ntohs(packet->arp_header->oper);
        packet->arp_header->spa = ntohl(packet->arp_header->spa);
        packet->arp_header->tpa = ntohl(packet->arp_header->tpa);
}

void hton_arp(arp_packet* packet) {
        packet->arp_header->htype = htons(packet->arp_header->htype);
        packet->arp_header->ptype = htons(packet->arp_header->ptype);
        packet->arp_header->oper = htons(packet->arp_header->oper);
        packet->arp_header->spa = htonl(packet->arp_header->spa);
        packet->arp_header->tpa = htonl(packet->arp_header->tpa);
}

// IP Functions //

void new_ip_packet(ip_packet* ip_packet, eth_packet* eth_packet, int validate) {
	ip_packet->eth_packet = eth_packet;
	ip_packet->ip_header = (ip_header*) (eth_packet->data);
	
        ntoh_ip(ip_packet);

        if (validate) {
		int ihl = ip_packet->ip_header->version_ihl & 0x0F;
		ip_packet->ping_header = (ping_header*) (((uint8_t*) eth_packet->data) + ihl * 4);
		ip_packet->tcp_header = (tcp_header*) (((uint8_t*) eth_packet->data) + ihl * 4);

                int valid = 1;
		valid &= ip_packet->ip_header->total_length >= sizeof(*ip_packet->ip_header);
		valid &= ip_packet->eth_packet->length >= sizeof(*eth_packet->header) + ip_packet->ip_header->total_length;
		valid &= ip_packet->eth_packet->length >= sizeof(*eth_packet->header) + ihl * 4;
		valid &= ip_packet->ip_header->protocol == 6 || ip_packet->ip_header->protocol == 1;

		ip_packet->length = ip_packet->ip_header->total_length;

                if (!valid) {
                        ip_packet->length = -1;
                        return;
                }
        } else {
		ip_packet->ping_header = (ping_header*) (((uint8_t*) eth_packet->data) + 20);
		ip_packet->tcp_header = (tcp_header*) (((uint8_t*) eth_packet->data) + 20);

		ip_packet->ip_header->version_ihl = 0x45;
		ip_packet->ip_header->dscp_ecn = 0;
		ip_packet->ip_header->total_length = 20;
		ip_packet->ip_header->identification = identification_counter++;
		ip_packet->ip_header->fragment_offset = 0x4000;
		ip_packet->ip_header->ttl = 128;
		ip_packet->ip_header->protocol = 6;
		ip_packet->ip_header->checksum = 0;
		ip_packet->ip_header->source_ip = 0;
		ip_packet->ip_header->dest_ip = 0;
        }
}

void handle_ip_packet(ip_packet rx_ip_packet) {
        if (rx_ip_packet.length < 0) {
                return;
        }

	if (rx_ip_packet.ip_header->protocol == 1) {
		handle_ping_packet(rx_ip_packet);
	} else if (rx_ip_packet.ip_header->protocol == 6) {
		handle_tcp_packet(rx_ip_packet);
	}
}

void ntoh_ip(ip_packet* packet) {
        packet->ip_header->total_length = ntohs(packet->ip_header->total_length);
        packet->ip_header->identification = ntohs(packet->ip_header->identification);
        packet->ip_header->fragment_offset = ntohs(packet->ip_header->fragment_offset);
        packet->ip_header->source_ip = ntohl(packet->ip_header->source_ip);
        packet->ip_header->dest_ip = ntohl(packet->ip_header->dest_ip);
}

void hton_ip(ip_packet* packet) {
        packet->ip_header->total_length = htons(packet->ip_header->total_length);
        packet->ip_header->identification = htons(packet->ip_header->identification);
        packet->ip_header->fragment_offset = htons(packet->ip_header->fragment_offset);
        packet->ip_header->source_ip = htonl(packet->ip_header->source_ip);
        packet->ip_header->dest_ip = htonl(packet->ip_header->dest_ip);
}

// Ping Functions //

void handle_ping_packet(ip_packet rx_ip_packet) {
        ntoh_ping(rx_ip_packet.ping_header);

	if (rx_ip_packet.ping_header->type != 8 || rx_ip_packet.ping_header->code != 0) {
		return;
	}

        eth_packet tx_packet;
        new_eth_packet(&tx_packet);

        tx_packet.type = ETH_IP_PACKET;

        ip_packet tx_ip_packet;
        new_ip_packet(&tx_ip_packet, &tx_packet, 0);

        tx_ip_packet.eth_packet->header->dest = rx_ip_packet.eth_packet->header->source;
        tx_ip_packet.eth_packet->header->source = get_local_mac();
        tx_ip_packet.eth_packet->header->ether_type = rx_ip_packet.eth_packet->header->ether_type;

        tx_ip_packet.ip_header->protocol = 1;
        tx_ip_packet.ip_header->total_length = rx_ip_packet.ip_header->total_length;
        tx_ip_packet.ip_header->source_ip = ip_addr;
        tx_ip_packet.ip_header->dest_ip = rx_ip_packet.ip_header->source_ip;

        tx_ip_packet.ping_header->type = 0;
	tx_ip_packet.ping_header->code = 0;
        tx_ip_packet.ping_header->checksum = 0;
	tx_ip_packet.ping_header->identifier = rx_ip_packet.ping_header->identifier;
	tx_ip_packet.ping_header->sequence_number = rx_ip_packet.ping_header->sequence_number;

        int ihl = rx_ip_packet.ip_header->version_ihl & 0xF;
        int payload_length = rx_ip_packet.ip_header->total_length - ihl * 4 - sizeof(*rx_ip_packet.ping_header);

        memcpy(tx_ip_packet.ping_header->payload, rx_ip_packet.ping_header->payload, payload_length);
        tx_ip_packet.ip_header->total_length = payload_length + 20 + sizeof(*rx_ip_packet.ping_header);
        tx_ip_packet.eth_packet->length = tx_ip_packet.ip_header->total_length + sizeof(*tx_ip_packet.eth_packet->header);

        hton_ip(&tx_ip_packet);
        hton_ping(tx_ip_packet.ping_header);

        tx_ip_packet.ip_header->checksum = htons(ip_checksum((uint8_t*) tx_ip_packet.ip_header, 20));
        tx_ip_packet.ping_header->checksum = htons(ip_checksum((uint8_t*) tx_ip_packet.ping_header, payload_length + sizeof(*rx_ip_packet.ping_header)));

        send_packet(tx_ip_packet.eth_packet);
}

void ntoh_ping(ping_header* header) {
        header->checksum = ntohs(header->checksum);
        header->identifier = ntohs(header->identifier);
        header->sequence_number = ntohs(header->sequence_number);
}

void hton_ping(ping_header* header) {
        header->checksum = htons(header->checksum);
        header->identifier = htons(header->identifier);
        header->sequence_number = htons(header->sequence_number);
}

// TCP packet

void handle_tcp_packet(ip_packet rx_ip_packet) {
        ntoh_tcp(rx_ip_packet.tcp_header);

	if (rx_ip_packet.tcp_header->dest_port != 80) {
		return;
	}

	if (tcp_conn.state != TCP_LISTEN) {
		int connection_match = 1;
		connection_match &= rx_ip_packet.tcp_header->source_port == tcp_conn.remote_port;
		connection_match &= rx_ip_packet.tcp_header->dest_port == tcp_conn.local_port;
		connection_match &= rx_ip_packet.ip_header->source_ip == tcp_conn.remote_ip;
		connection_match &= rx_ip_packet.ip_header->dest_ip == ip_addr;
		if (!connection_match) {
			return;
		}
	}

	if (rx_ip_packet.tcp_header->offset_flags & TCP_RST) {
		if (tcp_conn.state != TCP_LISTEN) {
			tcp_conn.state = TCP_SEND_RST;
                	tcp_conn.ack_timeout = -1;
		}
		return;
	}

	if (tcp_conn.state == TCP_LISTEN) {
		if (rx_ip_packet.tcp_header->offset_flags & TCP_SYN && !(rx_ip_packet.tcp_header->offset_flags & TCP_ACK)) {
			int data_offset = (rx_ip_packet.tcp_header->offset_flags >> 12) * 4;

                	int max_segment = scan_options(rx_ip_packet.tcp_header->options, data_offset - 20, 2);
                	int window_scale = scan_options(rx_ip_packet.tcp_header->options, data_offset - 20, 3);

			if (window_scale < 0) {
				window_scale = 0;
			}

			if (window_scale > 14) {
				return;
			}

			if (max_segment == 0) {
				return;
			}

			if (max_segment < 0 || max_segment > MAX_SEGMENT) {
				max_segment = MAX_SEGMENT;
			}

			tcp_conn.connection_id = connection_counter++;
			tcp_conn.state = TCP_SYN_RECEIVED;
			tcp_conn.local_port = rx_ip_packet.tcp_header->dest_port;
			tcp_conn.local_sequence = (rand() << 16) | (rand() & 0xFFFF);
			tcp_conn.remote_mac = rx_ip_packet.eth_packet->header->source;
			tcp_conn.remote_port = rx_ip_packet.tcp_header->source_port;
			tcp_conn.remote_ip = rx_ip_packet.ip_header->source_ip;
			tcp_conn.remote_sequence = rx_ip_packet.tcp_header->sequence;
			tcp_conn.remote_window_scale = window_scale;
			tcp_conn.remote_window = ((uint32_t) rx_ip_packet.tcp_header->window_size) << window_scale;
			tcp_conn.remote_max_segment = max_segment;
			tcp_conn.rx_write = 1;
			tcp_conn.rx_read = 1;
			tcp_conn.rx_fin = 0xFFFFFFFF;
			tcp_conn.tx_write = 1;
			tcp_conn.tx_sent = 1;
               		tcp_conn.tx_acked = 0;
			tcp_conn.tx_limit = 1 + tcp_conn.remote_window;
			tcp_conn.tx_fin = 0xFFFFFFFF;
			tcp_conn.ack_timeout = -1;
			tcp_conn.segment_count = 0;
		}
		return;
	}

        uint32_t base =  rx_ip_packet.tcp_header->sequence - tcp_conn.remote_sequence;

	int data_offset = (rx_ip_packet.tcp_header->offset_flags >> 12) * 4;

	int ihl = rx_ip_packet.ip_header->version_ihl & 0x0F;

	int data_length =  rx_ip_packet.ip_header->total_length - 4 * ihl - data_offset;

	if (data_length < 0 || data_length > MAX_SEGMENT) {
		return;
	} 

	if (tcp_conn.rx_write + data_length > tcp_conn.rx_read + TCP_WINDOW) {
		return;
	}

	int flags = rx_ip_packet.tcp_header->offset_flags & 0x0FFF;

	if (flags & TCP_SYN) {
		return;
	}

	if (!(flags & TCP_ACK)) {
		return;
	}

	add_segment(((uint8_t*) rx_ip_packet.tcp_header) + data_offset, data_length, base);

	int acked = 0;
	int ack = rx_ip_packet.tcp_header->ack - tcp_conn.local_sequence;

	if (ack > tcp_conn.tx_acked) {
		tcp_conn.tx_acked = ack;
		if (tcp_conn.tx_acked == tcp_conn.tx_fin) {
			tcp_conn.ack_timeout = -1;
		}
		acked = 1;
	}

        tcp_conn.remote_window = ((uint32_t) rx_ip_packet.tcp_header->window_size) << tcp_conn.remote_window_scale;
        uint32_t new_limit = tcp_conn.tx_acked + tcp_conn.remote_window;
        tcp_conn.tx_limit = (new_limit > tcp_conn.tx_limit) ? new_limit : tcp_conn.tx_limit;

        if (tcp_conn.state == TCP_SYN_ACK_SENT && acked) {
                tcp_conn.state = TCP_CONNECTED;
        }

	if (rx_ip_packet.tcp_header->offset_flags & TCP_FIN) {
		tcp_conn.rx_fin = base + data_length;
		tcp_conn.ack_timeout = -1;
	}

	if (data_length > 0) {
		tcp_conn.ack_timeout = -1;
	}
}

int add_segment(uint8_t* data, int data_length, uint32_t dest) {
	if (dest + data_length <= tcp_conn.rx_write) {
		return 0;
	}

	tcp_segment new_segment;
	new_segment.position = dest;
	new_segment.length = data_length;

	int latest_index = -1;
	uint32_t latest = 0;
	int i;
	for (i = 0; i < tcp_conn.segment_count; i++) {
		tcp_segment other = tcp_conn.rx_segments[i];
		if (other.position == new_segment.position && other.length == new_segment.length) {
			return 0;
		}
		if (other.position >= latest) {
			latest = i;
		}
	}

	copy_to_circle_buffer(tcp_conn.rx_buffer, RX_MASK, dest, data, data_length, data_length);

        if (tcp_conn.segment_count == SEG_MASK) {
                if (latest_index < 0) {
                        fprintf(stdout, "Illegal state: no latest segment when segment stack is full\n");
                        return 0;
                }
                if (latest_index >= tcp_conn.segment_count) {
                        fprintf(stdout, "Illegal state: latest state index exceeds stack size\n");
                        return 0;
                }
                tcp_segment latest_segment = tcp_conn.rx_segments[latest];
                if (latest_segment.position > new_segment.position) {
                        tcp_conn.rx_segments[latest] = new_segment;
                }
        } else {
		tcp_conn.rx_segments[tcp_conn.segment_count++] = new_segment;
	}

	int ack_required = 0;
	int updated = 1;
	while (updated) {
		updated = 0;
		int j = 0;
		for (i = 0; i < tcp_conn.segment_count; i++) {
			tcp_segment other = tcp_conn.rx_segments[i];
			if (other.position + other.length <= tcp_conn.rx_write) {
				// Drop segment
			} else if (other.position <= tcp_conn.rx_write) {
				tcp_conn.rx_write = other.position + other.length;

				updated = 1;
				ack_required = 1;
			} else {
				tcp_conn.rx_segments[j++] = other;
			}
		}
		tcp_conn.segment_count = j;
	}

	return ack_required;
}

int send_tcp_packet() {
	if (tcp_conn.state == TCP_LISTEN) {
		return 0;
	}

	int send = 0;
	send |= tcp_conn.tx_sent < tcp_conn.tx_write;
	send |= tcp_conn.ack_timeout == -1;
	send |= tcp_conn.state == TCP_SEND_RST;

	int timed_out = tcp_conn.tx_acked < tcp_conn.tx_sent && check_timeout(tcp_conn.ack_timeout);

	if (timed_out && !send) {
		send = 1;
		tcp_conn.tx_sent = tcp_conn.tx_acked;
		tcp_conn.tx_limit = tcp_conn.tx_acked + tcp_conn.remote_window;
	} else {
		timed_out = 0;
	}

	if (send) {
		int syn = 0;

        	eth_packet tx_packet;
        	new_eth_packet(&tx_packet);

        	tx_packet.type = ETH_IP_PACKET;

        	ip_packet tx_ip_packet;
        	new_ip_packet(&tx_ip_packet, &tx_packet, 0);

                if (tcp_conn.tx_acked == 0) {
                        syn = 1;
                }

                int data_length = tcp_conn.tx_write - tcp_conn.tx_sent;
		int data_offset = 20;

                if (syn) {
                        data_length = 0;
			data_offset = 28;
                        uint32_t* options = (uint32_t*) tx_ip_packet.tcp_header->options;
                        options[0] = htonl(0x02040000 + MAX_SEGMENT);
                        options[1] = htonl(0x01030300); // No windows size scaling
                }

		if (data_length > tcp_conn.tx_limit - tcp_conn.tx_sent) {
                        data_length = tcp_conn.tx_limit - tcp_conn.tx_sent;
                }

		if (data_length > MAX_SEGMENT) {
			data_length = MAX_SEGMENT;
		}

		int flags = TCP_ACK;

		if (syn) {
			flags |= TCP_SYN;
		}

		if (tcp_conn.tx_acked >= tcp_conn.tx_fin) {
			flags |= TCP_FIN;
		}

		if (tcp_conn.state == TCP_SEND_RST) {
			flags |= TCP_RST;
		}

        	tx_ip_packet.eth_packet->header->dest = tcp_conn.remote_mac;
        	tx_ip_packet.eth_packet->header->source = get_local_mac();
        	tx_ip_packet.eth_packet->header->ether_type = 0x0800;

        	tx_ip_packet.ip_header->total_length = 20 + data_offset + data_length;
        	tx_ip_packet.ip_header->source_ip = ip_addr;
        	tx_ip_packet.ip_header->dest_ip = tcp_conn.remote_ip;

        	tx_ip_packet.tcp_header->source_port = tcp_conn.local_port;
        	tx_ip_packet.tcp_header->dest_port = tcp_conn.remote_port;
		tx_ip_packet.tcp_header->sequence = tcp_conn.tx_sent + tcp_conn.local_sequence - (syn ? 1 : 0);
		tx_ip_packet.tcp_header->ack = tcp_conn.rx_write + tcp_conn.remote_sequence;
		tx_ip_packet.tcp_header->offset_flags = ((data_offset >> 2) << 12) | flags;
		tx_ip_packet.tcp_header->window_size = tcp_conn.rx_read + TCP_WINDOW - tcp_conn.rx_write;
		tx_ip_packet.tcp_header->checksum = 0;
		tx_ip_packet.tcp_header->urgent = 0;

		int window_size =  tx_ip_packet.tcp_header->window_size;

		copy_from_circle_buffer(((uint8_t*) tx_ip_packet.tcp_header) + data_offset, data_length, tcp_conn.tx_buffer, TX_MASK, tcp_conn.tx_sent, data_length);

		tx_packet.length = sizeof(*tx_packet.header) + tx_ip_packet.ip_header->total_length;

		tcp_conn.rx_acked = tcp_conn.rx_write;
                tcp_conn.rx_window = tx_ip_packet.tcp_header->window_size;

                hton_ip(&tx_ip_packet);
                hton_tcp(tx_ip_packet.tcp_header);

                tx_ip_packet.ip_header->checksum = htons(ip_checksum((uint8_t*) tx_ip_packet.ip_header, 20));

        	uint32_t tcp_checksum = ip_checksum_init();
        	tcp_checksum = ip_checksum_add(tcp_checksum, (char*) &(tx_ip_packet.ip_header->source_ip), 8);
        	char temp_encode[4];
        	temp_encode[0] = 0;
        	temp_encode[1] = 6;
        	int tcp_length = data_offset + data_length;
        	temp_encode[2] = tcp_length >> 8;
        	temp_encode[3] = tcp_length;
        	tcp_checksum = ip_checksum_add(tcp_checksum, temp_encode, 4);
        	tcp_checksum = ip_checksum_add(tcp_checksum, (uint8_t*) tx_ip_packet.tcp_header, data_offset + data_length);

        	tx_ip_packet.tcp_header->checksum = htons(ip_checksum_final(tcp_checksum));

       		send_packet(tx_ip_packet.eth_packet);
                tcp_conn.tx_sent = tcp_conn.tx_sent + data_length;

		tcp_conn.ack_timeout = get_now() + ACK_TIMEOUT;

		if (syn) {
			tcp_conn.state = TCP_SYN_ACK_SENT;
		}

		if (tcp_conn.state == TCP_SEND_RST) {
                        tcp_conn.state = TCP_DATA_TO_READ;
                }

		if (!tcp_conn.state == TCP_DATA_TO_READ) {
       	        	if (tcp_conn.rx_write >= tcp_conn.rx_fin) {
				if (tcp_conn.state == TCP_TX_CLOSED) {
       	                        	tcp_conn.state = TCP_BOTH_CLOSED;
       	                	} else {
       	                        	tcp_conn.state = TCP_RX_CLOSED;
				}
       	         	}
	
			if (flags & TCP_FIN) {
				if (tcp_conn.state == TCP_RX_CLOSED) {
					tcp_conn.state = TCP_BOTH_CLOSED;
				} else {
					tcp_conn.state = TCP_TX_CLOSED;
				}
			}
		}

                if (tcp_conn.state == TCP_BOTH_CLOSED) {
                        tcp_conn.state = TCP_SEND_RST;
                        tcp_conn.ack_timeout = -1;
                }

		return data_length > 0;
	}
	return 0;
}

int open_tcp() {
	if (tcp_conn.state == TCP_LISTEN) {
		return -1;
	}

	return tcp_conn.connection_id;
}

int read_tcp(int connection_id, uint8_t* buffer, int off, int len) {
	if (off < 0) {
		return -1;
	}

	if (tcp_conn.state == TCP_LISTEN || tcp_conn.connection_id != connection_id) {
		return -1;
	}

        if (tcp_conn.state == TCP_DATA_TO_READ) {
                if (tcp_conn.rx_read >= tcp_conn.rx_write || tcp_conn.rx_read >= tcp_conn.rx_fin) {
                        tcp_conn.state = TCP_LISTEN;
			return -1;
                }
        }

	if (tcp_conn.rx_read >= tcp_conn.rx_fin) {
		return -1;
	}

	int available = tcp_conn.rx_write - tcp_conn.rx_read;

	int data_length = (len < available) ? len : available;

	if (data_length < 0) {
		return -1;
	}

	copy_from_circle_buffer(buffer + off, len, tcp_conn.rx_buffer, RX_MASK, tcp_conn.rx_read, data_length);

	tcp_conn.rx_read += data_length;

	if (data_length > 0) {
		tcp_conn.ack_timeout = -1;
	}

	return data_length;
}

int write_tcp(int connection_id, uint8_t* buffer, int len) {
	if (tcp_conn.state == TCP_LISTEN || tcp_conn.connection_id != connection_id) {
                return -1;
        }

	int off = 0;
	int written = 0;
	while (len > 0) {
		int window_available = tcp_conn.tx_acked + tcp_conn.remote_window - tcp_conn.tx_write;
		int buffer_available = BUFFER_SIZE - (tcp_conn.tx_write - tcp_conn.tx_acked);

		int available = (window_available > buffer_available) ? buffer_available : window_available;

		int data_length = (len < available) ? len : available;

		if (data_length > 0) {
			copy_to_circle_buffer(tcp_conn.tx_buffer, TX_MASK, tcp_conn.tx_write, buffer + off, data_length, data_length);
			tcp_conn.tx_write += data_length;
			len -= data_length;
			written += data_length;
			off += data_length;
		}

		ip_stack_poll();
	}

	return written;
}

void close_tcp(int connection_id) {
	if (tcp_conn.connection_id != connection_id) {
		return;
	}

	tcp_conn.state = TCP_LISTEN;
}

int shutdown_tcp(int connection_id) {
	if (tcp_conn.state == TCP_LISTEN || tcp_conn.connection_id != connection_id) {
                return -1;
        }

	tcp_conn.tx_fin = tcp_conn.tx_write;
}

void ntoh_tcp(tcp_header* header) {
        header->source_port = ntohs(header->source_port);
        header->dest_port = ntohs(header->dest_port);
        header->sequence = ntohl(header->sequence);
        header->ack = ntohl(header->ack);
        header->offset_flags = ntohs(header->offset_flags);
        header->window_size = ntohs(header->window_size);
        header->urgent = ntohs(header->urgent);
}

void hton_tcp(tcp_header* header) {
        header->source_port = htons(header->source_port);
        header->dest_port = htons(header->dest_port);
        header->sequence = htonl(header->sequence);
        header->ack = htonl(header->ack);
        header->offset_flags = htons(header->offset_flags);
        header->window_size = htons(header->window_size);
        header->urgent = htons(header->urgent);
}

// Utility Functions //

long get_now() {
	struct timespec now_time;
	clock_gettime(CLOCK_MONOTONIC_RAW, &now_time);
	long now = (now_time.tv_sec * 1000) + (now_time.tv_sec / 1000000);
	if (!base_time_set) {
		base_time = now;
		base_time_set = 1;
	}
	return now - base_time;
}

int check_timeout(long timeout) {
	if (timeout == -1) {
		return 1;
	}

	return get_now() > timeout;
}

int32_t pack_ip_addr(int a, int b, int c, int d) {
        a &= 0xFF;
        b &= 0xFF;
        c &= 0xFF;
        d &= 0xFF;
        return (a << 24) | (b << 16) | (c << 8) | d;
}

char unpack_buffer[32];

char* unpack_ip_addr(uint32_t ip_addr) {
	int a = (ip_addr >> 24) & 0xFF;
	int b = (ip_addr >> 16) & 0xFF;
	int c = (ip_addr >> 8) & 0xFF;
	int d = (ip_addr) & 0xFF;
	sprintf(unpack_buffer, "%d.%d.%d.%d", a, b, c, d);
	return unpack_buffer;
}

int scan_options(uint8_t* buf, int len, int kind) {
        uint8_t* end = buf + len;
        for (; buf < end - 1; buf++) {
                if (*buf == 0) {
                        return -1;
                }
                if (*buf == 1) {
                        continue;
                }
                int size = *(buf + 1);
                if (buf + size > end) {
                        return -1;
                }
                if (*buf == kind) {
                        if (size == 4) {
				int msb = *(buf + 2);
				int lsb = *(buf + 3);
                                return (msb << 8) | lsb;
                        }
                        if (size == 3) {
                                return *(buf + 2);
                        }
                        return -1;
                }
                buf += size - 1;
        }
        return -1;
}

uint16_t ip_checksum(uint8_t* buf, int len) {
        uint32_t checksum = ip_checksum_init();
        checksum = ip_checksum_add(checksum, buf, len);
        return ip_checksum_final(checksum);
}

uint32_t ip_checksum_init() {
        return 0;
}

uint32_t ip_checksum_add(uint32_t checksum, uint8_t* buf, int len) {
        int i;
        for (i = 0; i < len - 1; i += 2) {
                int word = (buf[i] << 8) | buf[i + 1];
                checksum += word;
        }
        if (i != len) {
                int word = buf[len - 1] << 8;
                checksum += word;
        }
        return checksum;
}

uint32_t ip_checksum_final(uint32_t checksum) {
	checksum = (checksum & 0xFFFF) + (checksum >> 16);
	checksum = (checksum & 0xFFFF) + (checksum >> 16);
        return (~checksum) & 0xFFFF;
}

void copy_to_circle_buffer(uint8_t* dest, int dest_mask, int dest_start, uint8_t* source, int source_size, int length) {
	if (length > dest_mask) {
		return;
	}
	if (length > source_size) {
		return;
	}
	if (length <= 0) {
		return;
	}

	int mask_start = dest_start & dest_mask;
	int mask_end = (dest_start + length - 1) & dest_mask;

	if (mask_start <= mask_end) {
		memcpy(dest + mask_start, source, length);
	} else {
		int len1 = dest_mask + 1 - mask_start;
		int len2 = length - len1;
		memcpy(dest + mask_start, source, len1);
		memcpy(dest, source + len1, len2);
	}
}

void copy_from_circle_buffer(uint8_t* dest, int dest_size, uint8_t* source, int source_mask, int source_start, int length) {
        if (length > source_mask) {
                return;
        }
        if (length > dest_size) {
                return;
        }
        if (length <= 0) {
                return;
        }

        int mask_start = source_start & source_mask;
        int mask_end = (source_start + length - 1) & source_mask;

        if (mask_start <= mask_end) {
                memcpy(dest, source + mask_start, length); 
        } else {
                int len1 = source_mask + 1 - mask_start;
                int len2 = length - len1;

                memcpy(dest, source + mask_start, len1);
                memcpy(dest + len1, source, len2);
        }
}
