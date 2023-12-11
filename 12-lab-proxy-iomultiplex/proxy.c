#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <fcntl.h>
#include <sys/epoll.h>
#include <errno.h>
#include <sys/socket.h>

#define MAX_OBJECT_SIZE 102400
#define MAX_BUFFER_SIZE 16
#define NUM_THREADS 32

#define READ_REQUEST 1
#define SEND_REQUEST 2
#define READ_RESPONSE 3
#define SEND_RESPONSE 4

//static const char *user_agent_hdr = "User-Agent: Mozilla/5.0 (Macintosh; Intel Mac OS X 10.15; rv:97.0) Gecko/20100101 Firefox/97.0";

void test_parser();
void print_bytes(unsigned char *, int);
int open_sfd(const char *port_str);
int open_server(const char *hostname, const char *port);
struct request_info *make_default_struct();
void handle_new_clients(int sfd, int efd, struct epoll_event *event);
void handle_client(struct request_info *client_info, int epoll_fd);
int parse_request(char *, char *, char *, char *, char *);

struct request_info {
    int client_fd;
    int server_fd;
    int curr_state;
    char *client_in_buff;
    char *client_out_buff;
    char *server_in_buff;
    char *server_out_buff;
    int bytes_read_client;
    int bytes_written_client;
    int bytes_to_write_client;
    int bytes_read_server;
    int bytes_written_server;
};

int main(int argc, char *argv[]) {
    const char *port_str = (argc > 1) ? argv[1] : "8080";

    //printf("%s\n", user_agent_hdr);

    int sfd = open_sfd(port_str);
    int efd = epoll_create1(0);

    struct epoll_event event;
    event.events = EPOLLIN | EPOLLET;
    event.data.fd = sfd;

    if (epoll_ctl(efd, EPOLL_CTL_ADD, sfd, &event) < 0) {
        perror("epoll_ctl");
        exit(EXIT_FAILURE);
    }

    while (1) {
	    struct epoll_event events[64];
	    int num_events = epoll_wait(efd, events, 64, 1000);

	    if (num_events < 0) {
		perror("epoll_wait");
		exit(EXIT_FAILURE);
	    }

	    if (num_events == 0) {
		continue;
	    }

	    for (int i = 0; i < num_events; ++i) {
		if (events[i].data.fd == sfd) {
		    //printf("got new client\n");
	    	    // Event corresponds to the listening socket
		    handle_new_clients(sfd, efd, &events[i]);
		} else {
		    //printf("got old client\n");
		    // Event corresponds to an existing client
		    struct request_info *client_info = (struct request_info *)events[i].data.ptr;
		    handle_client(client_info, efd);
		}
	    }
    }
    close(sfd);
    close(efd);

    return 0;
}

int open_sfd(const char *port_str) {
    // Create a TCP socket
    int sfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sfd == -1) {
        perror("socket");
        exit(EXIT_FAILURE);
    }

    // Allow reuse of address and port
    int optval = 1;
    if (setsockopt(sfd, SOL_SOCKET, SO_REUSEPORT, &optval, sizeof(optval)) == -1) {
        perror("setsockopt");
        close(sfd);
        exit(EXIT_FAILURE);
    }

    if (fcntl(sfd, F_SETFL, fcntl(sfd, F_GETFL, 0) | O_NONBLOCK) < 0) {
        fprintf(stderr, "error setting socket option\n");
        exit(EXIT_FAILURE);
    }

    // Bind to the specified port
    struct sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(atoi(port_str));

    if (bind(sfd, (struct sockaddr *)&server_addr, sizeof(server_addr)) == -1) {
        perror("bind");
        close(sfd);
        exit(EXIT_FAILURE);
    }
    // Configure for accepting new clients
    if (listen(sfd, 10) == -1) {
        perror("listen");
        close(sfd);
        exit(EXIT_FAILURE);
    }

    return sfd;
}

int open_server(const char *hostname, const char *port) {
    struct addrinfo hints, *res;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    printf("hostname: %s, port: %s\n", hostname, port);
    if (getaddrinfo(hostname, port, &hints, &res) != 0) {
        perror("getaddrinfo");
        return -1;
    }

    int sockfd = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
    if (sockfd == -1) {
        perror("socket");
        freeaddrinfo(res);
        return -1;
    }

    if (connect(sockfd, res->ai_addr, res->ai_addrlen) == -1) {
        perror("connect");
        close(sockfd);
        freeaddrinfo(res);
        return -1;
    }

    freeaddrinfo(res);
    return sockfd;
}

struct request_info *make_default_struct() {
    struct request_info *def_struct;

    def_struct = (struct request_info *)malloc(sizeof(struct request_info));
    def_struct->client_fd = -1;
    def_struct->server_fd = -1;
    def_struct->curr_state = READ_REQUEST;
    def_struct->client_in_buff = (char *)malloc(2048 * sizeof(char));
    def_struct->client_out_buff = (char *)malloc(2048 * sizeof(char));
    def_struct->server_in_buff = (char *)malloc(MAX_OBJECT_SIZE * sizeof(char));
    def_struct->server_out_buff = (char *)malloc(MAX_OBJECT_SIZE * sizeof(char));
    def_struct->bytes_read_client = 0;
    def_struct->bytes_written_client = 0;
    def_struct->bytes_to_write_client = 0;
    def_struct->bytes_read_server = 0;
    def_struct->bytes_written_server = 0;

    return def_struct;
}

void handle_new_clients(int sfd, int efd, struct epoll_event *event) {
    while (1) {
        struct sockaddr_in in_addr;
        socklen_t in_addr_len = sizeof(in_addr);
        int client_fd = accept(sfd, (struct sockaddr *)&in_addr, &in_addr_len);

        if (client_fd < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                break;
            } else {
                perror("accept");
                exit(EXIT_FAILURE);
            }
        }

        int flags = fcntl(client_fd, F_GETFL, 0);
        flags |= O_NONBLOCK;

        if (fcntl(client_fd, F_SETFL, flags) < 0) {
            perror("fcntl");
            exit(EXIT_FAILURE);
        }

        struct request_info *client_info;
        client_info = make_default_struct();
        client_info->client_fd = client_fd;

        event->events = EPOLLIN | EPOLLET;
        event->data.ptr = client_info;

        if (epoll_ctl(efd, EPOLL_CTL_ADD, client_fd, event) < 0) {
            perror("epoll_ctl");
            close(client_fd);
            free(client_info);
            exit(EXIT_FAILURE);
        }
        printf("New client connected. File descriptor: %d\n", client_fd);
    }
}

void handle_client(struct request_info *client_info, int epoll_fd) {
    printf("Handling client on FD %d in state %d\n", client_info->client_fd, client_info->curr_state);

    if (client_info->curr_state == READ_REQUEST) {
        char buffer[2048];
	strcpy(buffer, client_info->client_in_buff);
	ssize_t bytes_received = client_info->bytes_read_client;
        ssize_t new_bytes;
        while (1) {
            new_bytes = read(client_info->client_fd, buffer + bytes_received, sizeof(buffer) - bytes_received);
	    printf("num bytes read from client: %ld\n", new_bytes);
	    if (new_bytes > 0) {
                bytes_received += new_bytes;
		client_info->bytes_read_client = bytes_received;

                // Check if the entire HTTP request has been received
                if (strstr(buffer, "\r\n\r\n") != NULL) {
                    // Print the HTTP request bytes
                    printf("Received HTTP request:\n");
                    // Null-terminate the HTTP request
                    buffer[bytes_received] = '\0';
		    printf("%s\n", buffer);
                    // Parse the HTTP request
                    char method[16], hostname[64], port[8], path[64];
		    if (parse_request(buffer, method, hostname, port, path)) {
    			printf("Method: %s\n", method);
			printf("Hostname: %s\n", hostname);
			printf("Port: %s\n", port);
			printf("Path: %s\n", path);

		    	char * beginning = strstr(buffer, "Host");
			char * end = (beginning != NULL) ? strstr(beginning, "\r\n\r\n") : NULL;	
			char * headers = malloc(1024);
			strncpy(headers, beginning, end - beginning);
			headers[end - beginning] = '\0';
			size_t total_length = snprintf(NULL, 0, "%s /%s HTTP/1.0\r\n%s\r\n\r\n", method, path, headers);
	
			char * server_request = (char*)malloc(total_length);
			snprintf(server_request,  total_length, "%s /%s HTTP/1.0\r\n%s\r\n\r\n", method, path, headers);
			server_request[total_length - 1] = '\n';
			
			// Print the request to be sent to the server
                        printf("Sending request to server:\n");
			printf("%s\n", server_request);
                        // Create a new socket and connect to the server
                        client_info->server_fd = open_server(hostname, port);
                        if (client_info->server_fd == -1) {
                            perror("connect");
                            close(client_info->client_fd);
                            free(client_info);
                            return;
                        }

                        // Configure the new socket as nonblocking
                        int flags = fcntl(client_info->server_fd, F_GETFL, 0);
                        flags |= O_NONBLOCK;
                        if (fcntl(client_info->server_fd, F_SETFL, flags) < 0) {
                            perror("fcntl");
                            close(client_info->server_fd);
                            close(client_info->client_fd);
                            free(client_info);
                            return;
                        }

                        // Unregister client-to-proxy socket from epoll
                        struct epoll_event ev;
                        ev.events = EPOLLIN | EPOLLET;
                        ev.data.fd = client_info->client_fd;
                        epoll_ctl(epoll_fd, EPOLL_CTL_DEL, client_info->client_fd, &ev);

                        // Register proxy-to-server socket with epoll for writing
                        ev.events = EPOLLOUT | EPOLLET;
                        ev.data.ptr = client_info;
                        epoll_ctl(epoll_fd, EPOLL_CTL_ADD, client_info->server_fd, &ev);

                        // Change the state to SEND_REQUEST
                        client_info->curr_state = SEND_REQUEST;
			
			client_info->client_in_buff = buffer;
			client_info->bytes_to_write_client = strlen(server_request);
			client_info->client_out_buff = server_request;

                        return;
                    }
                }
            } else if (new_bytes == 0) {
                // Connection closed by the client
                close(client_info->client_fd);
                free(client_info);
                return;
            } else {
                // Error reading from the socket
                if (errno == EAGAIN || errno == EWOULDBLOCK) {
                    // No more data ready to be read; continue when epoll notifies
		    client_info->client_in_buff = buffer;
		    client_info->bytes_read_client = bytes_received;
		    return;
                } else {
                    perror("read");
                    return;
                }
            }
        }
    }
    else if (client_info->curr_state == SEND_REQUEST) {
        printf("Handling send request state\n");
        ssize_t bytes_sent;
	//client_info->bytes_to_write_client = client_info->bytes_read_client;
        printf("total bytes to send: %d\n", client_info->bytes_to_write_client - client_info->bytes_written_client);
	while (1) {
            bytes_sent = write(client_info->server_fd, client_info->client_out_buff + client_info->bytes_written_client, client_info->bytes_to_write_client - client_info->bytes_written_client);
	    client_info->bytes_written_client += bytes_sent;
            if (bytes_sent > 0) {
                if (client_info->bytes_written_client == client_info->bytes_to_write_client) {
                    // Sent entire HTTP request to the server
                    struct epoll_event ev;
                    ev.events = EPOLLOUT | EPOLLET;
                    ev.data.ptr = client_info;
                    epoll_ctl(epoll_fd, EPOLL_CTL_DEL, client_info->server_fd, &ev);

                    ev.events = EPOLLIN | EPOLLET;
                    epoll_ctl(epoll_fd, EPOLL_CTL_ADD, client_info->server_fd, &ev);

                    client_info->curr_state = READ_RESPONSE;
			
		    printf("Done writing all of the bytes\n");
                    return;
                }
            } else if (bytes_sent == 0) {
                // Connection closed by the server
                printf("bytes sent equal to 0\n");
		close(client_info->server_fd);
                close(client_info->client_fd);
                free(client_info);
                return;
            } else {
                // Error writing to the socket
                printf("bytes sent less than 0\n");
                if (errno == EAGAIN || errno == EWOULDBLOCK) {
                    // No more buffer space available; continue when epoll notifies
                    return;
                } else {
                    perror("write");
                    close(client_info->server_fd);
                    close(client_info->client_fd);
                    free(client_info);
                    return;
                }
            }
        }
    }
    else if (client_info->curr_state == READ_RESPONSE) {
        printf("Handling read response\n");
	ssize_t new_bytes = 0;
        while (1) {
            new_bytes = read(client_info->server_fd, client_info->server_in_buff + client_info->bytes_read_server, 2048);
            if (new_bytes > 0) {
	    	client_info->bytes_read_server += new_bytes;
            } else if (new_bytes == 0) {
                // Connection closed by the server
                close(client_info->server_fd);
                printf("Received complete HTTP response:\n");
                //printf("%s\n", client_info->server_in_buff);
		//print_bytes((unsigned char *)client_info->server_in_buff, client_info->bytes_read_server);

                // Register client-to-proxy socket with epoll for writing
                struct epoll_event ev;
                ev.events = EPOLLOUT | EPOLLET;
                ev.data.ptr = client_info;
                epoll_ctl(epoll_fd, EPOLL_CTL_ADD, client_info->client_fd, &ev);

                // Change state to SEND_RESPONSE
                client_info->curr_state = SEND_RESPONSE;
                return;
            } else {
                // Error reading from the socket
		if (errno == EAGAIN || errno == EWOULDBLOCK) {
                    // No more data ready to be read; continue when epoll notifies
                    printf("no more data read\n");
		    return;
                } else {
                    perror("read");
                    close(client_info->server_fd);
                    close(client_info->client_fd);
                    free(client_info);
                    return;
                }
            }
        }
    }    
    else if (client_info->curr_state == SEND_RESPONSE) {
	    printf("Handling send response state\n");
	    ssize_t bytes_sent;
	    while (1) {
		// Check if there is anything left to send
		if (client_info->bytes_written_server == client_info->bytes_read_server) {
		    // Entire HTTP response has been sent to the client
		    printf("Entire HTTP response sent to the client\n");
		    
		    // Free memory associated with the current struct request_info
		    // and close the client-to-proxy socket
		    close(client_info->client_fd);
		    free(client_info);

		    // You are done!
		    return;
		}
		printf("writing bytes to the client\n");
		// Continue writing the HTTP response to the client socket
		bytes_sent = write(client_info->client_fd,
				   client_info->server_in_buff + client_info->bytes_written_server,
				   client_info->bytes_read_server - client_info->bytes_written_server);

		if (bytes_sent > 0) {
		    // Update the bytes_written_server counter
		    client_info->bytes_written_server += bytes_sent;
		} else if (bytes_sent == 0) {
		    // Connection closed by the client
		    close(client_info->client_fd);
		    free(client_info->client_in_buff);
		    free(client_info->client_out_buff);
		    free(client_info->server_in_buff);
		    free(client_info->server_out_buff);
		    free(client_info);
		    return;
		} else {
		    // Error writing to the socket
		    if (errno == EAGAIN || errno == EWOULDBLOCK) {
			// No more buffer space available; continue when epoll notifies
			return;
		    } else {
			perror("write");
			close(client_info->client_fd);
			free(client_info);
			return;
		    }
		}
	    }
	}
}

int complete_request_received(char *request) {
    if (strstr(request, "\r\n\r\n") != NULL) {
        return 1;
    }
    return 0;
}

int parse_request(char *request, char *method,
		char *hostname, char *port, char *path) {
	if (complete_request_received(request) == 1) {
		char * beginning = request;
		char * end = strstr(beginning, " ");
		strncpy(method, beginning, end - beginning);
		method[end - beginning] = '\0';
		beginning = end + 1;
		if (strstr(beginning, "://") != NULL) {
		    end = strstr(beginning, "://");
		    beginning = end + 3;
		}
		end = strstr(beginning, " ");
		char * url = malloc(1024);		
		strncpy(url, beginning, end - beginning);
		char * contains_colon = strstr(url, ":");
		if (contains_colon != NULL) {
			end = strstr(beginning, ":");
			strncpy(hostname, beginning, end - beginning);
			hostname[end - beginning] = '\0';
			beginning = end + 1;
			end = strstr(beginning, "/");
			strncpy(port, beginning, end - beginning);
			port[end - beginning] = '\0';
		}
		else {
			end = strstr(beginning, "/");
			strncpy(hostname, beginning, end - beginning);
			hostname[end - beginning] = '\0';
			strcpy(port, "80");
		}
		beginning = end + 1;
		end = strstr(beginning, " ");
		strncpy(path, beginning, end - beginning);
		path[end - beginning] = '\0';
		return 1;
	}
	return 0;
}
void test_parser() {
	int i;
	char method[16], hostname[64], port[8], path[64];

       	char *reqs[] = {
		"GET http://www.example.com/index.html HTTP/1.0\r\n"
		"Host: www.example.com\r\n"
		"User-Agent: Mozilla/5.0 (X11; Linux x86_64; rv:68.0) Gecko/20100101 Firefox/68.0\r\n"
		"Accept-Language: en-US,en;q=0.5\r\n\r\n",

		"GET http://www.example.com:8080/index.html?foo=1&bar=2 HTTP/1.0\r\n"
		"Host: www.example.com:8080\r\n"
		"User-Agent: Mozilla/5.0 (X11; Linux x86_64; rv:68.0) Gecko/20100101 Firefox/68.0\r\n"
		"Accept-Language: en-US,en;q=0.5\r\n\r\n",

		"GET http://localhost:1234/home.html HTTP/1.0\r\n"
		"Host: localhost:1234\r\n"
		"User-Agent: Mozilla/5.0 (X11; Linux x86_64; rv:68.0) Gecko/20100101 Firefox/68.0\r\n"
		"Accept-Language: en-US,en;q=0.5\r\n\r\n",

		"GET http://www.example.com:8080/index.html HTTP/1.0\r\n",

		NULL
	};
	
	for (i = 0; reqs[i] != NULL; i++) {
		printf("Testing %s\n", reqs[i]);
		if (parse_request(reqs[i], method, hostname, port, path)) {
			printf("METHOD: %s\n", method);
			printf("HOSTNAME: %s\n", hostname);
			printf("PORT: %s\n", port);
			printf("PATH: %s\n", path);
		} else {
			printf("REQUEST INCOMPLETE\n");
		}
	}
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
