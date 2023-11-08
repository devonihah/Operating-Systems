// Replace PUT_USERID_HERE with your actual BYU CS user id, which you can find
// by running `id -u` on a CS lab machine.
#define USERID 1823693159

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <arpa/inet.h>

int verbose = 0;
char* get_hex_string(int decimal);
int get_hex_representation(int decimal);
void print_bytes(unsigned char *bytes, int byteslen);

int main(int argc, char *argv[]) {
	char* server = (char *)malloc(64 * sizeof(char));
	int port = -1;
	int level = -1;
	int seed = -1;
	if (argc < 5) {
		printf("Not enough arguments");
		return -1;
	}
	server = argv[1];
	port = atoi(argv[2]);
	level = atoi(argv[3]);
	seed = atoi(argv[4]);
	//printf("server: %s, port: %d, level: %d, seed: %d\n", server, port, level, seed);

	unsigned char buf[64];

	bzero(buf, 64);
	
	int hex_user_id = htonl(USERID);
	seed = htons(seed);
	
	memcpy(&buf[1], &level, sizeof(int));
	memcpy(&buf[2], &hex_user_id, sizeof(int));
	memcpy(&buf[6], &seed, sizeof(64));

	print_bytes(buf, 8);
}

char* get_hex_string(int decimal) {
	char *hex_string = (char*)malloc(64 * sizeof(char));
	sprintf(hex_string, "%x", decimal);
	printf("%s\n", hex_string);
	return hex_string;
}

int get_hex_representation(int decimal) {
	char hex_string[64];
	unsigned int hex_value;
	sprintf(hex_string, "%x", decimal);
	printf("%s\n", hex_string);
	sscanf(hex_string, "%x", &hex_value);
	printf("%d\n", hex_value);
	return hex_value;
}


void print_bytes(unsigned char *bytes, int byteslen) {
	int i, j, byteslen_adjusted;

	if (byteslen % 8) {
		byteslen_adjusted = ((byteslen / 8) + 1) * 8;
	} else {
		byteslen_adjusted = byteslen;
	}
	for (i = 0; i < byteslen_adjusted + 1; i++) {
		if (!(i % 8)) {
			if (i > 0) {
				for (j = i - 8; j < i; j++) {
					if (j >= byteslen_adjusted) {
						printf("  ");
					} else if (j >= byteslen) {
						printf("  ");
					} else if (bytes[j] >= '!' && bytes[j] <= '~') {
						printf(" %c", bytes[j]);
					} else {
						printf(" .");
					}
				}
			}
			if (i < byteslen_adjusted) {
				printf("\n%02X: ", i);
			}
		} else if (!(i % 4)) {
			printf(" ");
		}
		if (i >= byteslen_adjusted) {
			continue;
		} else if (i >= byteslen) {
			printf("   ");
		} else {
			printf("%02X ", bytes[i]);
		}
	}
	printf("\n");
}
