#ifndef IP_STACK_H
#define IP_STACK_H

#include <stdint.h>

void ip_stack_poll();

int init_ip(uint32_t addr);
int close_ip();

int open_tcp();
int read_tcp(int connection_id, uint8_t* buffer, int off, int len);
int write_tcp(int connection_id, uint8_t* buffer, int len);
int shutdown_tcp(int connection_id);
void close_tcp(int connection_id);

int32_t pack_ip_addr(int a, int b, int c, int d);
char* unpack_ip_addr(uint32_t addr);

#endif
