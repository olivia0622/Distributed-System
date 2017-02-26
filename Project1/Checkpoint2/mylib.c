#define _GNU_SOURCE

#include <dlfcn.h>
#include <stdio.h>
 
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdarg.h>
#include <stdlib.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <string.h>
#include <unistd.h>
#include <err.h>
#include <errno.h>
#define MAXMSGLEN 10000
#define MAXRESSIZE 2000

// The following line declares a function pointer with the same prototype as the open function.  
int (*orig_open)(const char *pathname, int flags, ...);  // mode_t mode is needed when flags includes O_CREAT
int (*orig_close)(int fd);
ssize_t (*orig_read)(int fd, void *buf, size_t count);
ssize_t (*orig_write)(int fd, const void *buf, size_t count);
off_t (*orig_lseek)(int fd, off_t offset, int whence);
int (*orig_stat)(int ver, const char * path, struct stat * stat_buf);
int (*orig_unlink)(const char *path);
ssize_t (*orig_getdirentries)(int fd, char *buf, size_t nbytes , off_t *basep);
struct dirtreenode* (*orig_getdirtree)(const char *path);
void (*orig_freedirtree)(struct dirtreenode* dt);
void send_message(void *msg);
void pack_header(char* request, size_t request_len, int flags, int opcode);
void pack_open(char* request, mode_t mode, const char* content);
void pack_write(char* request, const void* buf, size_t count);
void pack_read(char* request, size_t count);
void get_response(int fd, char* request, int request_len, char* response, int response_len);
void connect_to_server(void);


int sockfd;
// This is our replacement for the open function from libc.
// Decode: size, opcode,  flags.
void pack_header(char* request, size_t request_len, int flags, int opcode) {	
	int addr = sizeof(int);
	memcpy(request, &request_len, sizeof(int));
	memcpy(request + addr, &opcode, sizeof(int));
	addr = addr + sizeof(int);
	memcpy(request + addr, &flags, sizeof(int));
}
void pack_open(char* request, mode_t mode, const char* content) {
	int addr = 3 * sizeof(int);
	memcpy(request + addr, &mode, sizeof(mode_t));
	addr = addr + sizeof(mode_t);
	memcpy(request + addr, content, strlen(content));
}
void pack_write(char* request, const void* buf, size_t count) {
	int addr = 3 * sizeof(int);
	memcpy(request + addr, &count, sizeof(size_t));
	addr = addr + sizeof(size_t);
	memcpy(request + addr, buf, count);
}
void pack_read(char* request, size_t count) {
	int addr = 3 * sizeof(int);
	memcpy(request + addr, &count, sizeof(size_t));
}

int open(const char *pathname, int flags, ...) {
	size_t response_len = 0;
	char response[MAXRESSIZE];
	mode_t m=0;
	if (flags & O_CREAT) {
		va_list a;
		va_start(a, flags);
		m = va_arg(a, mode_t);
		va_end(a);
	}
	size_t request_len = sizeof(int) * 3 + sizeof(mode_t) + strlen(pathname);
	char* request = malloc(request_len);
	pack_header(request, request_len, flags, 0);
	pack_open(request, m, pathname);
	//printf("before response: %d, %s\n", request_len, pathname);
	get_response(sockfd, request, request_len, response, response_len);

	int offset = sizeof(int);
	int return_info = *(int* ) (response + offset);
	errno = *(int *) (response + 3 * offset);
	fprintf(stderr, "errno: %d\n", errno);
	return return_info;
}

int close(int fd) {
	size_t response_len = 0;
	char response[MAXRESSIZE];
	size_t request_len = sizeof(int) * 3;
	char* request = malloc(request_len);
	
    pack_header(request, request_len, fd, 1);
	get_response(sockfd, request, request_len, response, response_len);
    fprintf(stderr, "fd: %d\n", fd);
	int offset = sizeof(int);
	int return_info = *(int* ) (response + offset);
    fprintf(stderr, "ret: %d\n", return_info);
	errno = *(int *) (response + 3 * offset);
	return return_info;
}

ssize_t write(int fd, const void *buf, size_t count) {

	size_t response_len = 0;
	char response[MAXRESSIZE];	
	size_t request_len = sizeof(int) * 3 + sizeof(size_t) + count;
	char* request = malloc(request_len);
	pack_header(request, request_len, fd, 2);
	pack_write(request, buf, count);
	fprintf(stderr, "buf: %s\n", (char *)(request+ sizeof(int) *3 + sizeof(size_t)));	
	fprintf(stderr, "in write, request_len:%d, fd: %d, count:%d \n", *(int *)(request), *(int*)(request+sizeof(int)*2), count);
	fprintf(stderr, "opcode: %d\n", *(int *)(request+ sizeof(int)));	
	get_response(sockfd, request, request_len, response, response_len);
        
	int offset = sizeof(int);
	ssize_t return_info = *(ssize_t *) (response + offset);
	fprintf(stderr, "return length: %d\n", return_info);

    offset += sizeof(ssize_t) + sizeof(int);
	errno = *(int *)(response + offset);
	return return_info;

}
ssize_t read(int fd, void *buf, size_t count) {
	size_t response_len = 0;
	char response[MAXRESSIZE];
	size_t request_len = sizeof(int) * 3 + sizeof(size_t);
	char* request = malloc(request_len);
	pack_header(request, request_len, fd, 3);
	pack_read(request, count);
	get_response(sockfd, request, request_len, response, response_len);

	int offset = sizeof(int);
	ssize_t return_info = *(ssize_t *)(response + offset);
	offset += sizeof(ssize_t) + sizeof(int);
	errno = *(int* )(response + offset);
	if (return_info != -1) {
		offset += sizeof(int);
		memcpy(buf, response + offset, count);
		fprintf(stderr, "buf: %s\n", buf);
	}
	return return_info;
}
off_t lseek(int fd, off_t offset, int whence) {
	send_message("lseek");
	return orig_lseek(fd, offset, whence);
}
int __xstat(int ver, const char * path, struct stat * stat_buf) {
	send_message("stat");
	return orig_stat(ver, path, stat_buf);
}
int unlink(const char *path) {
	send_message("unlink");
	return orig_unlink(path);
}
ssize_t getdirentries(int fd, char *buf, size_t nbytes , off_t *basep) {
	send_message("getdirentries");
	return orig_getdirentries(fd, buf, nbytes, basep);
}
struct dirtreenode* getdirtree(const char *path) {
	send_message("getdirtree");
	return orig_getdirtree(path);
}
void freedirtree(struct dirtreenode* dt) {
	send_message("freedirtree");
	return orig_freedirtree(dt);

}

void send_message(void *msg) {
	send(sockfd, msg, strlen(msg), 0);
}
void get_response(int fd, char* request, int request_len, char* response, int response_len) {
	char buf[MAXRESSIZE];
	send(fd, request, request_len, 0);
	response_len = -1;
	int rv = recv(fd, buf, MAXRESSIZE, 0);
	if (rv < 0) err(1, 0);
	memcpy(response, buf, rv);
}

// This function is automatically called when program is started
void _init(void) {
	// set function pointer orig_open to point to the original open function
	orig_open = dlsym(RTLD_NEXT, "open");
	orig_close = dlsym(RTLD_NEXT, "close");
	orig_read = dlsym(RTLD_NEXT, "read");
	orig_write = dlsym(RTLD_NEXT, "write");
	orig_lseek = dlsym(RTLD_NEXT, "lseek");
	orig_stat = dlsym(RTLD_NEXT, "__xstat");
	orig_unlink = dlsym(RTLD_NEXT, "unlink");
	orig_getdirtree = dlsym(RTLD_NEXT, "getdirtree");
	orig_getdirentries = dlsym(RTLD_NEXT, "getdirentries");
	orig_freedirtree = dlsym(RTLD_NEXT, "freedirtree");
	connect_to_server();
}

void connect_to_server(void) {
	char *serverip;
	char *serverport;
	unsigned short port;
	int rv;
	struct sockaddr_in srv;
	
	// Get environment variable indicating the ip address of the server
	serverip = getenv("server15440");
	if (serverip);
	else {
		serverip = "128.2.220.11";
	}
	
	// Get environment variable indicating the port of the server
	serverport = getenv("serverport15440");
	if (serverport);
	else {
		serverport = "15442";
	}
	port = (unsigned short)atoi(serverport);
	
	// Create socket
	sockfd = socket(AF_INET, SOCK_STREAM, 0);	// TCP/IP socket
	if (sockfd<0) err(1, 0);			// in case of error
	
	// setup address structure to point to server
	memset(&srv, 0, sizeof(srv));			// clear it first
	srv.sin_family = AF_INET;			// IP family
	srv.sin_addr.s_addr = inet_addr(serverip);	// IP address of server
	srv.sin_port = htons(port);			// server port

	// actually connect to the server
	rv = connect(sockfd, (struct sockaddr*)&srv, sizeof(struct sockaddr));
	if (rv<0) err(1,0);
	

}


