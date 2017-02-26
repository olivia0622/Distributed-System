/**
*Name : Mimi Chen
*AndrewID : mimic1
* This is file for the server side.
**/
#define _GNU_SOURCE

#include <dlfcn.h>
#include <stdio.h>
#include <sys/select.h>
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
#include <dirent.h>
#include "../include/dirtree.h"

#define MAXBODY 10000

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
void pack_lseek(char* request, off_t offset, int whence);
void pack_xstat(char* request, const char* path);
void pack_unlink(char* request, const char* path);
void pack_getdirentries(char* request, size_t nbytes, off_t *basep);
void pack_getdirtree(char* request, size_t request_len, const char* path, int opcode);
struct dirtreenode* unpack_tree(char* response, size_t response_len, int* offset);
void connect_to_server(void);
void send_request(int fd, char* request, int request_len);
int get_length(int fd, size_t* response_len);
void get_response(int fd, char* response, size_t* response_len);

int sockfd;

// Encode: size, opcode,  flags.
void pack_header(char* request, size_t request_len, int flags, int opcode) {	
	int addr = sizeof(int);
	memcpy(request, &request_len, sizeof(int));
	memcpy(request + addr, &opcode, sizeof(int));
	addr = addr + sizeof(int);
	memcpy(request + addr, &flags, sizeof(int));
}
// Encode open content;
void pack_open(char* request, mode_t mode, const char* content) {
	int addr = 3 * sizeof(int);
	memcpy(request + addr, &mode, sizeof(mode_t));
	addr = addr + sizeof(mode_t);
	memcpy(request + addr, content, strlen(content));
}
// Encode write.
void pack_write(char* request, const void* buf, size_t count) {
	int addr = 3 * sizeof(int);
	memcpy(request + addr, &count, sizeof(size_t));
	addr = addr + sizeof(size_t);
	memcpy(request + addr, buf, count);
}
// Encode read.
void pack_read(char* request, size_t count) {
	int addr = 3 * sizeof(int);
	memcpy(request + addr, &count, sizeof(size_t));
}
// Encode lseek.
void pack_lseek(char* request, off_t offset, int whence) {
	int addr = 3 * sizeof(int);
	memcpy(request + addr, &offset, sizeof(off_t));
	addr = addr + sizeof(off_t);
	memcpy(request + addr, &whence, sizeof(int));
}
//Encode xstat.
void pack_xstat(char* request, const char* path) {
	int addr = 3*sizeof(int);
	int len = strlen(path);
	memcpy(request + addr, &len, sizeof(int));
	addr = addr + sizeof(int);
	memcpy(request + addr, path, strlen(path));
}
//Encode unlink
void pack_unlink(char* request, const char* path) {
	int addr = 3*sizeof(int);
	memcpy(request +addr, path, strlen(path));
}
//Encode getdirentries
void pack_getdirentries(char* request, size_t nbytes, off_t *basep) {
	int addr = 3 * sizeof(int);
	memcpy(request + addr, &nbytes, sizeof(size_t));
	addr = addr + sizeof(size_t);
	memcpy(request + addr, basep, sizeof(off_t));
}
//Encode getdirtree
void pack_getdirtree(char* request, size_t request_len, const char* path, int opcode) {
	int addr = sizeof(int);
	memcpy(request, &request_len, sizeof(int));
	memcpy(request + addr, &opcode, sizeof(int));
	addr = addr + sizeof(int);
	memcpy(request + addr, path, strlen(path));
}
//Unpack the message from the server.
struct dirtreenode* unpack_tree(char* response, size_t response_len, int* offset) {
	if (response_len == 0) {
		return NULL;
	}
	int size = *(int* )(response + *offset);
	struct dirtreenode *child = (struct dirtreenode *)malloc(sizeof(struct dirtreenode));
	child->name = malloc(size + 1);
	*offset += sizeof(int);
	memcpy(child->name, response + *offset, size + 1);

	*offset += size + 1;
	int num = *(int* )(response + *offset);
	child->num_subdirs = num;

	if (num > 0 ){
		child->subdirs = malloc(num * sizeof(struct dirtreenode*));
	}
	*offset += sizeof(int);
	int i = 0;
	for (i = 0; i < num; i++) {
		child->subdirs[i] = unpack_tree(response, response_len - *offset, offset);
	}
	return child;
}
/*
*Send open RPC.
* pack the request content
* send the request to client 
* get the response from client
* unpack the response
*/
int open(const char *pathname, int flags, ...) {
	size_t response_len = 0;
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

	send_request(sockfd, request, request_len);
	int rv = get_length(sockfd, &response_len);
	char* response = malloc(response_len - sizeof(size_t));
	response_len -= rv;

	get_response(sockfd, response, &response_len);

	int offset = 3 * sizeof(size_t);
	int return_info = *(int* ) (response);
	if (return_info < 0) {
		errno = *(int *) (response + 3 * offset);	
	}
	free(response);
	if(return_info >= 0) {
		return return_info + 1024;
	}
	return return_info;
}
/*
*Send close RPC.
* pack the request content
* send the request to client 
* get the response from client
* unpack the response
*/
int close(int fd) {
	if (fd < 1024) {
		return orig_close(fd);
	}
	fd -= 1024;
	size_t response_len = 0;
	size_t request_len = sizeof(int) * 3;
	char* request = malloc(request_len);
    pack_header(request, request_len, fd, 1);

	send_request(sockfd, request, request_len);
	int rv = get_length(sockfd, &response_len);
	
	char* response = malloc(response_len - sizeof(size_t));
	response_len -= rv;

	get_response(sockfd, response, &response_len);

	int offset = 2* sizeof(size_t);
	int return_info = *(int* ) (response);
	if (return_info < 0) {
		errno = *(int *) (response + offset);
	}
	free(response);
	return return_info;
}
/*
*Send write RPC.
* pack the request content
* send the request to client 
* get the response from client
* unpack the response
*/
ssize_t write(int fd, const void *buf, size_t count) {
	if (fd < 1024) {
		return orig_write(fd, buf, count);
	}
	
	fd -= 1024;

	size_t response_len = 0;
	size_t request_len = sizeof(int) * 3 + sizeof(size_t) + count;
	char* request = malloc(request_len);
	pack_header(request, request_len, fd, 2);
	pack_write(request, buf, count);

	send_request(sockfd, request, request_len);
	int rv = get_length(sockfd, &response_len);
	
	char* response = malloc(response_len - sizeof(size_t));
	response_len -= rv;

	get_response(sockfd, response, &response_len);
        
	ssize_t return_info = *(ssize_t *) (response);
    int offset = sizeof(ssize_t) + sizeof(int);
	if (return_info < 0) {
		errno = *(int *)(response + offset);
	}
	free(response);

	return return_info;
}
/*
*Send read RPC.
* pack the request content
* send the request to client 
* get the response from client
* unpack the response
*/
ssize_t read(int fd, void *buf, size_t count) {
	if (fd < 1024) {
		return orig_read(fd, buf, count);
	}
	
	fd -= 1024;
	size_t response_len = 0;
	size_t request_len = sizeof(int) * 3 + sizeof(size_t);
	char* request = malloc(request_len);
	pack_header(request, request_len, fd, 3);
	pack_read(request, count);

	send_request(sockfd, request, request_len);
	int rv = get_length(sockfd, &response_len);
	
	char* response = malloc(response_len - sizeof(size_t));
	response_len -= rv;

	get_response(sockfd, response, &response_len);

	ssize_t return_info = *(ssize_t *)(response);
	int offset = sizeof(ssize_t) + sizeof(int);
	if (return_info < 0) {
		errno = *(int* )(response + offset);
	}
	if (return_info > 0) {
		offset += sizeof(int);
		memcpy(buf, response + offset, return_info);
	}
	free(response);

	return return_info;
}
/*
*Send lseek RPC.
* pack the request content
* send the request to client 
* get the response from client
* unpack the response
*/
off_t lseek(int fd, off_t offset, int whence) {
	if (fd < 1024) {
		return orig_lseek(fd, offset, whence);
	}
	
	fd -= 1024;
	size_t response_len = 0;
	size_t request_len = sizeof(int) * 4 + sizeof(off_t);
	char* request = malloc(request_len);
	pack_header(request, request_len, fd, 4);
	pack_lseek(request, offset, whence);

	send_request(sockfd, request, request_len);
	int rv = get_length(sockfd, &response_len);
	
	char* response = malloc(response_len - sizeof(size_t));
	response_len -= rv;

	get_response(sockfd, response, &response_len);

	//int off = sizeof(size_t);
	off_t return_info = *(off_t *)(response);
	int off = sizeof(off_t) + sizeof(int);
	if (return_info < 0) {
		errno = *(int* )(response + off);
	}
	free(response);
	return return_info;
}
/*
*Send xstat RPC.
* pack the request content
* send the request to client 
* get the response from client
* unpack the response
*/
int __xstat(int ver, const char * path, struct stat * stat_buf) {
	size_t response_len = 0;
	size_t request_len = sizeof(int) * 4 + strlen(path);
	char* request = malloc(request_len);
	pack_header(request, request_len, ver, 5);
	pack_xstat(request, path);

	send_request(sockfd, request, request_len);
	int rv = get_length(sockfd, &response_len);
	char* response = malloc(response_len - sizeof(size_t));
	response_len -= rv;

	get_response(sockfd, response, &response_len);

	int return_info = *(int *)(response );
	int offset = sizeof(int) * 2;
	if (return_info < 0) {
		errno = *(int* )(response + offset);
	}
	if (return_info >= 0) {
		offset += sizeof(int);
		memcpy(stat_buf, response + offset, sizeof(struct stat));
	}
	free(response);

	return return_info;
}
/*
*Send unlink RPC.
* pack the request content
* send the request to client 
* get the response from client
* unpack the response
*/
int unlink(const char *path) {
	size_t response_len = 0;
	size_t request_len = sizeof(int) * 3 + strlen(path);
	char* request = malloc(request_len);
	pack_header(request, request_len, strlen(path), 6);
	pack_unlink(request, path);
	send_request(sockfd, request, request_len);
	int rv = get_length(sockfd, &response_len);
	
	char* response = malloc(response_len - sizeof(size_t));
	response_len -= rv;

	get_response(sockfd, response, &response_len);

	int return_info = *(int* )(request);
	int offset = sizeof(int) * 2;
	if (return_info < 0) {
		errno = *(int* )(response + offset);
	}
	free(response);

	return return_info;
}
/*
*Send getdirentries RPC.
* pack the request content
* send the request to client 
* get the response from client
* unpack the response
*/
ssize_t getdirentries(int fd, char *buf, size_t nbytes , off_t *basep) {
	if (fd < 1024) {
		return orig_getdirentries(fd, buf, nbytes, basep);
	}
	
	fd -= 1024;
	size_t response_len = 0;
	size_t request_len = sizeof(int) * 3 + sizeof(size_t) + sizeof(off_t);
	char* request = malloc(request_len);
	pack_header(request, request_len, fd, 7);
	pack_getdirentries(request, nbytes, basep);

	send_request(sockfd, request, request_len);
	int rv = get_length(sockfd, &response_len);
	
	char* response = malloc(response_len - sizeof(size_t));
	response_len -= rv;

	get_response(sockfd, response, &response_len);

	ssize_t return_info = *(ssize_t *)(response);
	int offset = sizeof(ssize_t) + sizeof(int);
	if (return_info < 0) {
		errno = *(int* )(response + offset);
	}
	offset += sizeof(int);
	if (return_info > 0) {
		memcpy(buf, response + offset, return_info);
	}
	offset += return_info;
	memcpy(basep, response + offset, sizeof(off_t));
	free(response);

	return return_info;
}
/*
*Send getdirtree RPC.
* pack the request content
* send the request to client 
* get the response from client
* unpack the response
*/
struct dirtreenode* getdirtree(const char *path) {
	size_t response_len = 0;
	size_t request_len = sizeof(int) * 2 + strlen(path);
	char* request = malloc(request_len);
	pack_getdirtree(request, request_len, path, 8);

	send_request(sockfd, request, request_len);
	int rv = get_length(sockfd, &response_len);
	
	char* response = malloc(response_len - sizeof(size_t));
	response_len -= rv;

	get_response(sockfd, response, &response_len);

	int offset = sizeof(int);
	errno = *(int* )(response + offset);
	offset += sizeof(int);
	int init = 0;
	struct dirtreenode* tree = unpack_tree(response + offset, response_len - offset, &init);
	free(response);
	return tree;
}
void freedirtree(struct dirtreenode* dt) {
	orig_freedirtree(dt);
}

void send_message(void *msg) {
	send(sockfd, msg, strlen(msg), 0);
}
//Send request.
void send_request(int fd, char* request, int request_len) {
	int send_length = 0;
	while (request_len > 0) {
		int sv = send(fd, request + send_length, request_len, 0);
		request_len -= sv;
		send_length += sv;
	}
	free(request);
}
//Get the response from server.
int get_length(int fd, size_t* response_len) {
	int len = sizeof(size_t);
	char buf[10];
	int rv = recv(fd, buf, len, 0);
	*response_len = *(size_t* )buf;
	return rv;
}
//Get the content from the response
void get_response(int fd, char* response, size_t* response_len) {
	char buf[MAXBODY + 1];
	size_t recv_len = 0;
	int rv;
	while (( rv = recv(fd, buf, MAXBODY, 0)) > 0) {
		memcpy(response + recv_len, buf, rv);
		recv_len += rv;
		if (recv_len >= *response_len) break;
		
	}	
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
		serverport = "15311";
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


