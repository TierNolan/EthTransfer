#ifndef WEB_SERVER_H
#define WEB_SERVER_H

#include "stdint.h"

#define LF (10)
#define CR (13)

int header_scan(uint8_t* buffer, int len);
void split_header(uint8_t* buffer, int len);
int header_value(uint8_t* buffer, int len, char* key);
void send_response(int connection_id, char* response);
void send_http_message(int connection_id, char* message);
void send_http_upload_form(int connection_id);
void send_http_download_link(int connection_id, char* filename);
void send_http_file(int connection_id, char* filename);
void shutdown_close(int connection_id);
char* parse_http_content(uint8_t* buffer, int len, char* boundary);
int find_needle(uint8_t* haystack, int haystack_length, uint8_t* needle, int needle_length);

#endif
