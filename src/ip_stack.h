#ifndef IP_STACK_H
#define IP_STACK_H

#include <stdint.h>

int init_ip();
int close_ip();

int open_tcp();
int read_tcp(int connection_id, uint8_t* buffer, int len);
int write_tcp(int connection_id, uint8_t* buffer, int len);
int shutdown_tcp(int connection_id);
void close_tcp();

#endif
