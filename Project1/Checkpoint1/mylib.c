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
#define MAXMSGLEN 100

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
int connect_to_server(void);
// This is our replacement for the open function from libc.
int open(const char *pathname, int flags, ...) {
	mode_t m=0;
	if (flags & O_CREAT) {
		va_list a;
		va_start(a, flags);
		m = va_arg(a, mode_t);
		va_end(a);
	}
	// we just print a message, then call through to the original open function (from libc)
	//fprintf(stderr, "mylib: open called for path %s\n", pathname);
    send_message("open");
	return orig_open(pathname, flags, m);
}
int close(int fd) {
	send_message("close");
	return orig_close(fd);
}
ssize_t read(int fd, void *buf, size_t count) {
	send_message("read");
	return orig_read(fd, buf, count);
}
ssize_t write(int fd, const void *buf, size_t count) {
	send_message("write");
	return orig_write(fd, buf, count);

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
	int fd = connect_to_server();
	send(fd, msg, strlen(msg), 0);
	orig_close(fd);
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

	//fprintf(stderr, "Init mylib\n");
}

int connect_to_server(void) {
	char *serverip;
	char *serverport;
	unsigned short port;
	//char *msg="Hello from client";
	//char buf[MAXMSGLEN+1];
	int sockfd, rv;
	struct sockaddr_in srv;
	
	// Get environment variable indicating the ip address of the server
	serverip = getenv("server15440");
	if (serverip){ 
	}else {
		serverip = "128.2.220.20";
	}
	
	// Get environment variable indicating the port of the server
	serverport = getenv("serverport15440");
	if (serverport){ 
	}else {
		serverport = "15440";
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
	
	// send message to server
	//send(sockfd, msg, strlen(msg), 0);	// send message; should check return value
	
	// get message back
	//rv = recv(sockfd, buf, MAXMSGLEN, 0);	// get message
	//if (rv<0) err(1,0);			// in case something went wrong
	//buf[rv]=0;				// null terminate string to print	

	return sockfd;
}


