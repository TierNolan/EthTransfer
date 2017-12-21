#ifndef ETH_DRIVER_H
#define ETH_DRIVER_H

#include "gpio_spi.h"

int init_ethernet();
int close_ethernet();

int get_packet(char* buffer, int len);
int queue_packet(char *buffer, int len);
int poll_tx_send();


#endif
