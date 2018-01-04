#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "web_server.h"
#include "ip_stack.h"

int main(int argc, char **argv) {
	int max_size = 1 * 1024 * 1024;

	int32_t ip_addr = 0xA9FE1234;

	char* filename = NULL;

	int j;
	for (j = 1; j < argc; j++) {
		if (strcmp(argv[j], "-h") == 0) {
			printf("Usage: %s [-h] [-i address] [-m size] [filename]\n", argv[0]);
			printf("       -h:         shows this help info\n");
			printf("       -i:         Sets the ip address\n");
			printf("       -m:         Sets the maximum file size\n");
			printf("       filename:   Sets the filename to send\n");
			printf("\n");
			printf("Sending a file\n");
			printf("\n");
			printf("%s my_file.txt\n", argv[0]);
			printf("\n");
			printf("Receiving a file\n");
			printf("\n");
			printf("%s\n", argv[0]);
			exit(0);
		}
		if (j < argc - 1) {
			if (strcmp(argv[j], "-i") == 0) {
				int a, b, c, d;
				int fields = sscanf(argv[j + 1], "%d.%d.%d.%d", &a, &b, &c, &d);
				if (fields != 4) {
					printf("Unable to parse %s as an IP address\n", argv[j + 1]);
					exit(-1);
				}
				ip_addr = pack_ip_addr(a, b, c, d);
				j++;
				continue;
			} else if (strcmp(argv[j], "-m") == 0) {
				int size;
				int fields = sscanf(argv[j + 1], "%d", &size);
				if (fields != 1) {
					printf("Unable to parse %s as a maximum size\n", argv[j + 1]);
					exit(1);
				}
				if (max_size > 128 * 1024 * 1024) {
					printf("Maximum size may not be set above 128MB\n");
					exit(-1);
				}
				max_size = size;
			}
		}
		if (filename != NULL) {
			printf("More than 1 filename specified\n");
			exit(-1);
		}
		filename = argv[j];
	}

        if (!init_ip(ip_addr)) {
                fprintf(stderr, "Unable to init ip stack\n");
                return 1;
        }

        uint8_t* read_buffer;

	read_buffer = malloc(max_size * sizeof(*read_buffer));

	if (read_buffer == NULL) {
		fprintf(stderr, "Unable to allocate space for read buffer\n");
		exit(-1);
	}

	int filename_length = filename == NULL ? 0 : strlen(filename);

	if (filename_length > 100) {
		printf("Maximum filename length exceeded\n");
		exit(-1);
	}

	printf("Starting Server at http://%s/\n", unpack_ip_addr(ip_addr));

	if (filename == NULL) {
		printf("Waiting for upload\n");
	} else {
		FILE* fp;
		fp = fopen(filename, "r");
		if (fp == NULL) {
			printf("Unable to find file %s\n", filename);
			exit(-1);
		}
		fclose(fp);
		printf("Serving %s\n", filename);
	}

	int read_count = 0;

	int http_header = -1;

	int content_length = -1;

	int required_length = -1;

        int connection_id = -1;

	int complete = 0;

	char* boundary = NULL;

        while (!complete) {
                ip_stack_poll();

                if (connection_id == -1) {
			http_header = -1;
			read_count = 0;
			content_length = -1;
			required_length = -1;
                        connection_id = open_tcp();
                } else {
			int max_data = max_size - read_count;

			if (max_data <= 0) {
				printf("Read buffer exhausted\n");
				exit(-1);
			}

                        int read = read_tcp(connection_id, read_buffer, read_count, max_data);

			if (read >= 0) {
				read_count += read;
			}

			if (http_header == -1) {
				boundary = NULL;

				http_header = header_scan(read_buffer, read_count);
				if (http_header == -1) {
					continue;
				} 
				read_buffer[http_header - 1] = 0;

				split_header(read_buffer, http_header);

				if (strncmp(read_buffer, "GET ", 4) == 0) {
					if (strncmp(read_buffer + 4, "/ ", 2) == 0) {
						if (filename == NULL) {
							send_http_upload_form(connection_id);
						} else {
							send_http_download_link(connection_id, filename);
						}
					} else if (*(read_buffer + 4) == '/' && strncmp(read_buffer + 5, filename, filename_length) == 0 && *(read_buffer + 5 + filename_length) == ' ') {
						send_http_file(connection_id, filename);
						printf("File transfer complete\n");
						complete = 1;
					} else {
						send_http_message(connection_id, "File not found\n");
					}
					connection_id = -1;
					continue;
				} else if (strncmp(read_buffer, "POST ", 5) == 0) {
					int content_index = header_value(read_buffer, http_header, "Content-Length: ");
					if (content_index == -1) {
                                        	send_http_message(connection_id, "No content length field found in html post header");
                                		connection_id = -1;
						continue;
					}
					if (sscanf(read_buffer + content_index, "%d", &content_length) != 1) {
						send_http_message(connection_id, "Unable to parse content length field");
                                		connection_id = -1;
						continue;
					}
					if (content_length < 0) {
						send_http_message(connection_id, "Negative content length");
						connection_id = -1;
						continue;
					}
					if (content_length > max_size ) {
						send_http_message(connection_id, "Content length exceeds max size parameter");
						connection_id = -1;
						continue;
					}
					int content_type = header_value(read_buffer, http_header, "Content-Type: ");
					if (content_type == -1) {
						send_http_message(connection_id, "No content type field found in html post header");
						connection_id = -1;
						continue;
					}
					boundary = strstr(read_buffer + content_type, "boundary=");
					if (boundary == NULL) {
						send_http_message(connection_id, "No boundary field in content type field");
						connection_id = -1;
						continue;
					}
					boundary += strlen("boundary=");
					required_length = content_length + http_header;
					if (required_length > max_size - 1) {
						send_http_message(connection_id, "Total length exceeds max size parameter");
						connection_id = -1;
						continue;
					}
				}
			}

			if (read_count >= required_length) {
				read_buffer[required_length] = 0;

				char *fail_message = parse_http_content(read_buffer + http_header, required_length - http_header, boundary);

				if (fail_message != NULL) {
					send_http_message(connection_id, fail_message);
				} else {
					send_http_message(connection_id, "Upload complete");
					complete = 1;
				}
				connection_id = -1;
                                continue;
			}
                }
        }

        close_ip();

        return 0;
}

void send_http_message(int connection_id, char* message) {
	char content[1024];

	sprintf(content, "%s\n%s\n%s\n",
                                "<html><head></head><body>",
				message,
                                "</body></html>");

	char response[1024];

	sprintf(response, "HTTP/1.1 200 OK\r\nContent-Length: %d\r\nContent-Type: text/html\r\nConnection: closed\r\n\r\n%s", strlen(content) - 1, content);

	send_response(connection_id, response);
}

void send_http_upload_form(int connection_id) {
	char content[1024];

	sprintf(content, "%s\n%s\n%s\n%s\n%s\n%s\n%s\n",
                                "<html><head></head><body>",
                                "<form enctype=\"multipart/form-data\" action=\"upload.html\" method=\"POST\">",
                                "Select file to upload<br>",
				"<input type=\"submit\" value=\"Upload\" />",
				"<input name=\"uploadfile\" type=\"file\"/><br/>",
                                "</form>",
                                "</body></html>");

	char response[1024];

	sprintf(response, "HTTP/1.1 200 OK\r\nContent-Length: %d\r\nContent-Type: text/html\r\nConnection: closed\r\n\r\n%s", strlen(content), content);

	send_response(connection_id, response);
}

void send_http_download_link(int connection_id, char* filename) {
	char message[1024];
	sprintf(message, "<a href=\"%s\">%s</a>", filename, filename);
	send_http_message(connection_id, message);
}

void send_response(int connection_id, char* response) {
        int len = strlen(response);

        write_tcp(connection_id, response, len);

	shutdown_close(connection_id);
}

void send_http_file(int connection_id, char* filename) {
	FILE* fp = fopen(filename, "rb");

	if (fp == NULL) {
		send_http_message(connection_id, "Unable to find file");
		return;
	}

	fseek(fp, 0L, SEEK_END);
	long length = ftell(fp);

	if (length > 512 * 1024 * 1024) {
		send_http_message(connection_id, "File is more than 512MB\n");
		fclose(fp);
		return;
	}

        char response[1024];

        sprintf(response, "HTTP/1.1 200 OK\r\nContent-Length: %d\r\nContent-Type: application/octet-stream\r\nContent-Disposition: attachment; filename=\"%s\"\r\nConnection: closed\r\n\r\n", length, filename);

	write_tcp(connection_id, response, strlen(response));

	fseek(fp, 0L, SEEK_SET);

	char buf[4096];

	int count = 0;
	int read = 0;
	while (read >= 0 && count < length) {
		read = fread(buf, 1, sizeof(buf) - 1, fp); 
		buf[read] = 0;
		if (read > 0) {
			write_tcp(connection_id, buf, read);
			count += read;
		}
		ip_stack_poll();
	}

	shutdown_close(connection_id);
}

void shutdown_close(int connection_id) {
	shutdown_tcp(connection_id);

        uint8_t null_buf[4096];

	int eof = 0;
	while (!eof) {
		int read = 1;
		do {
			read = read_tcp(connection_id, null_buf, 0, sizeof(null_buf));
		} while (read > 0);
		eof = read < 0;
                ip_stack_poll();
        }

        close_tcp(connection_id);
}

char* parse_http_content(uint8_t* buffer, int len, char* boundary) {
	int boundary_length = strlen(boundary);

	int begin = find_needle(buffer, len, boundary, boundary_length);
	if (begin == -1) {
		return "Unable to find first boundary marker in header";
	}
	int end = find_needle(buffer + begin + boundary_length, len - begin - boundary_length, boundary, boundary_length);
	if (end == -1) {
		return "Unable to find second boundary market in header";
	}
	begin += boundary_length;

	end += begin;

	if (buffer[end - 4] == CR) {
		end -= 4;
	} else {
		end -= 3;
	}

	buffer[end] = 0;

	int content_length = end - begin;

	char* header = buffer + begin;

	int content_header = header_scan(header, content_length);

	if (content_header < 0) {
		return "No header found in content";
	}

	split_header(header, content_header);

	int content_disposition = header_value(header, content_header, "Content-Disposition: ");

	if (content_disposition < 0) {
		return "No Content-Disposition found in content header";
	}

	char *filename_field = strstr(header + content_disposition, "filename=");

	if (filename_field == NULL) {
		return "No filename field found in Content-Disposition field";
	}

	filename_field += strlen("filename=");

	char *t = filename_field;

	while (*t != 0 && *t != ';') {
		t++;
	}

	*t = 0;	

	if (filename_field[0] == '"')
		filename_field++;

	if (filename_field[strlen(filename_field) - 1] == '"') {
		filename_field[strlen(filename_field) - 1] = 0;
	}

	printf("File uploaded\n");
	printf("Please confirm file save\n");
	printf("Filename: %s\n", filename_field);
	printf("Size:     %d\n", content_length - content_header);

	int confirmed = 0;

	int failed = 0;

	char user_input[16];
	while (!confirmed) {
		if (failed > 5) {
			return "Fail count exceed for user confirmation";
		}

		printf("Do you want to save this file? (yes/no)\n");
		char* read = fgets(user_input, sizeof(user_input), stdin);

		if (read == NULL) {
			return "Failed to query user for confirmation";
		}

		user_input[15] = 0;
		if (strncmp(user_input, "no", 2) == 0) {
			return "Transfer rejected by user";
		} else if (strncmp(user_input, "yes", 3) == 0) {
			confirmed = 1;
		} else {
			printf("%s is not a valid option\n", user_input);
		}

		failed++;
	}

	FILE* fp;
	fp = fopen(filename_field, "wb");

	if (fp == NULL) {
		return "Unable to open file for writing";
	}

	fwrite(header + content_header, 1, content_length - content_header, fp);

	fclose(fp);
			
	return NULL;
}

int header_scan(uint8_t* buffer, int len) {
	if (len < 0) {
		return -1;
	}
	if (len > 1024 * 1024) {
		len = 1024 * 1024;
	}
	int i = 0;
	for (i= 0; i < len; i++) {
		if (buffer[i] == LF) {
			if (i < len - 1 && buffer[i + 1] == LF) {
				return i + 2;
			}
			if (i < len - 2 && buffer[i + 1] == CR && buffer[i + 2] == LF) {
				return i + 3;
			}
		}
	}
	return -1;
}

int find_needle(uint8_t* haystack, int haystack_length, uint8_t* needle, int needle_length) {
	int i;
	for (i = 0; i < haystack_length - needle_length; i++) {
		if (haystack[i] == needle[0]) {
			int match = 1;
			int j;
			for (j = 0; j < needle_length; j++) {
				if (haystack[i + j] != needle[j]) {
					match = 0;
					j = needle_length;
				}
			}
			if (match) {
				return i;
			}
		}
	}
	return -1;
}

void split_header(uint8_t* buffer, int len) {
	int i;
	for (i = 0; i < len; i++) {
		if (buffer[i] == LF || buffer[i] == CR) {
			buffer[i] = 0;
		}
	}
}

int header_value(uint8_t* buffer, int len, char* key) {
	int key_len = strlen(key);
	int line_index = 0;

	while (line_index < len - key_len) {
		uint8_t* line = buffer + line_index;
		if (strlen(line) >= key_len && strncmp(key, line, key_len) == 0) {
			return line_index + key_len;
		}
		while (line_index < len - key_len && buffer[line_index] != 0) {
			line_index++;
		}
		line_index++;	
	}

	return -1;
}
