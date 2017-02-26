#include <stdio.h>
#include <stdlib.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <string.h>
#include <unistd.h>
#include <err.h>
#include <errno.h>
#include <fcntl.h>

#define MAXMSGLEN 100
#define MAXREQSIZE 2000
char* pack_int_response(int opcode, int ret, size_t *response_len);
char* pack_write_response(int opcode, ssize_t ret, size_t *response_len);
char* pack_read_response(int opcode, ssize_t ret3, size_t *response_len, char* content);
int handle_open(char* request, size_t request_len);
int handle_close(char* request, size_t request_len);
ssize_t handle_write(char* request, size_t request_len);
ssize_t handle_read(char* request, size_t request_len, char* buf);


int main(int argc, char**argv) {
	char buf[MAXREQSIZE+1];
	char *serverport;
	unsigned short port;
	int sockfd, sessfd, rv;
	struct sockaddr_in srv, cli;
	socklen_t sa_size;
	
	// Get environment variable indicating the port of the server
	serverport = getenv("serverport15440");
	if (serverport) port = (unsigned short)atoi(serverport);
	else port=15442;
	
	// Create socket
	sockfd = socket(AF_INET, SOCK_STREAM, 0);	// TCP/IP socket
	if (sockfd<0) err(1, 0);			// in case of error
	
	// setup address structure to indicate server port
	memset(&srv, 0, sizeof(srv));			// clear it first
	srv.sin_family = AF_INET;			// IP family
	srv.sin_addr.s_addr = htonl(INADDR_ANY);	// don't care IP address
	srv.sin_port = htons(port);			// server port

	// bind to our port
	rv = bind(sockfd, (struct sockaddr*)&srv, sizeof(struct sockaddr));
	if (rv<0) err(1,0);
	
	// start listening for connections
	rv = listen(sockfd, 5);
	if (rv<0) err(1,0);
	
	// main server loop, handle clients one at a time, quit after 10 clients
	while(1) {
		int request_len = -1;
		int opcode = -1;
		//char request[MAXREQSIZE];
		// wait for next client, get session socket
		sa_size = sizeof(struct sockaddr_in);
		sessfd = accept(sockfd, (struct sockaddr *)&cli, &sa_size);

		if (sessfd<0) err(1,0);
		// get messages and send replies to this client, until it goes away
		while ((rv=recv(sessfd, buf, MAXREQSIZE, 0)) > 0) {
			int offset = 0;
			fprintf(stderr, "rv: %d\n", rv);
			request_len = *(int *) buf;
			offset += sizeof(int);
			opcode = *(int* )(buf + offset);
			offset += sizeof(int);
			fprintf(stderr, "opcode: %d\n", opcode);
			size_t response_len = 0;
			char* response;
			int ret0, ret1;
			ssize_t ret2, ret3;
			switch(opcode) {
				case 0 :
					fprintf(stdout, "open\n");
					ret0 = handle_open(buf + offset, request_len - offset);
					response = pack_int_response(opcode, ret0, &response_len);
					break;
			 	case 1 :
					fprintf(stdout, "close\n");
					ret1 = handle_close(buf + offset, request_len - offset);
					response = pack_int_response(opcode, ret1, &response_len);
					break;
				case 2 :
					fprintf(stdout, "write\n");
					ret2 = handle_write(buf + offset, request_len - offset);
					response = pack_write_response(opcode, ret2, &response_len);
					break;
				case 3 :
					fprintf(stdout, "read\n");
					char* content = malloc(*(int *)(buf + offset + sizeof(int)) + 1);
					ret3 = handle_read(buf + offset, request_len - offset, content);
					response = pack_read_response(opcode, ret3, &response_len, content);
					free(content);
					break;
			}
			send(sessfd, response, response_len, 0);
			free(response);
		}
		// either client closed connection, or error
		if (rv<0) err(1,0);
		//close(sessfd);
	}
	
	close(sockfd);

	return 0;
}

char* pack_int_response(int opcode, int ret, size_t *response_len) {
	int offset = sizeof(int);
	*response_len = 4 * sizeof(int);
	char* response = malloc(*response_len);
	memcpy(response, &response_len, sizeof(int));
	memcpy(response + offset, &ret, sizeof(int));
	offset += sizeof(int);
	memcpy(response + offset, &opcode, sizeof(int));
	offset += sizeof(int);
	memcpy(response + offset, &errno, sizeof(int));
	return response;
}
char* pack_write_response(int opcode, ssize_t ret, size_t *response_len) {
	int offset = sizeof(int);
	*response_len = 3 * sizeof(int) + sizeof(ssize_t);
	char* response = malloc(*response_len);
	memcpy(response, &response_len, sizeof(int));
	memcpy(response + offset, &ret, sizeof(ssize_t));
	offset += sizeof(ssize_t);
	memcpy(response + offset, &opcode, sizeof(int));
	offset += sizeof(int);
	memcpy(response + offset, &errno, sizeof(int));
	return response;
}
char* pack_read_response(int opcode, ssize_t ret3, size_t *response_len, char* content) {
	int offset = sizeof(int);
	*response_len = 3 * sizeof(int) + sizeof(ssize_t) + strlen(content);
	char* response = malloc(*response_len);
	memcpy(response, &response_len, sizeof(int));
	memcpy(response + offset, &ret3, sizeof(ssize_t));
	offset += sizeof(ssize_t);
	memcpy(response + offset, &opcode, sizeof(int));
	offset += sizeof(int);
	memcpy(response + offset, &errno, sizeof(int));
	offset += sizeof(int);
	memcpy(response + offset, content, strlen(content));
	return response;

}

int handle_open(char* request, size_t request_len) {
	int offset = sizeof(int);
	int flags = *(int *) request;
	mode_t mode = *(mode_t *) (request + offset);
	char* file = malloc(request_len - offset + 1);
	file[request_len - offset] = 0; // this is for the end of a file;
	offset += sizeof(mode_t);
	memcpy(file, request+offset, request_len - offset);
	fprintf(stderr, "filename: %s  mode: %d flags: %d\n", request + offset, mode, flags);
	int ret = open(file, flags, mode);
    fprintf(stderr, "finished open: %d\n", ret);
	free(file);
	return ret;
}

int handle_close(char* request, size_t request_len) {
	int fd = *(int *) request;
	int ret = close(fd);
	return ret;
}

ssize_t handle_write(char* request, size_t request_len) {
    fprintf(stderr, "begin write\n");
	int offset = sizeof(int);
	int fd = *(int *) request;
	size_t count = *(size_t *) (request + offset);
	offset += sizeof(size_t);
	char* buf = malloc(count + 1);
	buf[count] = 0; // this is for the end of a file;
	fprintf(stderr, "count: %d, fd: %d\n", count, fd);
	memcpy(buf, request + offset, count);
	ssize_t ret = write(fd, buf, count);
	free(buf);
	return ret;
}

ssize_t handle_read(char* request, size_t request_len, char* buf) {
	int offset = sizeof(int);
	int fd = *(int *) request;
        size_t count = *(size_t *)(request + offset);
	//buf = malloc(count + 1);
	buf[count] = 0;
	ssize_t ret = read(fd, buf, count);
	return ret;
}





