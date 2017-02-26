/**
*Name : Mimi Chen
*AndrewID : mimic1
* This is file for the server side.
**/
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
#include <sys/stat.h>
#include <dirent.h>
#include "../include/dirtree.h"

#define MAXREQSIZE 2000
char* pack_int_response(int opcode, int ret, size_t *response_len);
char* pack_write_response(int opcode, ssize_t ret, size_t *response_len);
char* pack_read_response(int opcode, ssize_t ret3, size_t *response_len, char* content);
char* pack_lseek_response(int opcode, off_t ret, size_t *response_len);
char* pack_xstat_response(int opcode, int ret5, size_t *response_len, struct stat * stat_buf);
char* pack_unlink_response(int opcode, int ret6, size_t *response_len);
char* pack_getdirentries_response(int opcode, ssize_t ret7, size_t *response_len, char* content, off_t *basep);
char* pack_getdirtree_response(int opcode, char* body, size_t* response_len, int size);
int handle_open(char* request, size_t request_len);
int handle_close(char* request, size_t request_len);
ssize_t handle_write(char* request, size_t request_len);
ssize_t handle_read(char* request, size_t request_len, char* buf);
off_t handle_lseek(char* request, size_t request_len);
int handle_xstat(char* request, size_t request_len, struct stat * stat_buf);
int handle_unlink(char* request, size_t request_len);
ssize_t handle_getdirentries(char* request, int request_len, char* content, off_t* basep);
struct dirtreenode* handle_getdirtree(char* request, int request_len);
void encode_body(struct dirtreenode* tree, char* body, int* size);
void get_size(struct dirtreenode* tree, int* size);


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
	else port=15311;
	
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
        int offset = 0;
		int recv_len = 0;
		
		// wait for next client, get session socket
		sa_size = sizeof(struct sockaddr_in);
		sessfd = accept(sockfd, (struct sockaddr *)&cli, &sa_size);

		if (sessfd<0) err(1,0);
		pid_t pid;
		if ((pid = fork()) == 0) {
			close(sockfd);
			ssize_t rv_sub = 0;
			
			char* request;
			// get messages and send replies to this client, until it goes away
			while ((rv_sub = recv(sessfd, buf, MAXREQSIZE, 0)) > 0) {
			    //use the request_len to mark that the request is the first request
			    // with the opcode.
				if (request_len == -1) {
					request_len = *(int *) buf;
					request = malloc(request_len);
					offset += sizeof(int);
					opcode = *(int* )(buf + offset);
					offset += sizeof(int);
					
				}
				memcpy(request + recv_len, buf, rv_sub);

				recv_len += rv_sub;
				if (recv_len < request_len) continue;
				size_t response_len = 0;
				// int ret0, ret1, ret5, ret6;
				// ssize_t ret2, ret3,ret7;
				// off_t ret4;
				char* response;
				//The code for different operations.
				switch(opcode) {
					case 0 : {
						fprintf(stdout, "open\n");
						int ret0 = handle_open(request + offset, request_len - offset);
						response = pack_int_response(opcode, ret0, &response_len);
						break;
					}
				 	case 1 : {
						fprintf(stdout, "close\n");
						int ret1 = handle_close(request + offset, request_len - offset);
						response = pack_int_response(opcode, ret1, &response_len);
						break;
					}
					case 2 : {
						fprintf(stdout, "write\n");
						ssize_t ret2 = handle_write(request + offset, request_len - offset);
						response = pack_write_response(opcode, ret2, &response_len);
						break;
					}
					case 3 : {
						fprintf(stdout, "read\n");
						char* content = malloc(*(int *)(request + offset + sizeof(int)));
						ssize_t ret3 = handle_read(request + offset, request_len - offset, content);
						response = pack_read_response(opcode, ret3, &response_len, content);
						free(content);
						break;
					}
					case 4 : {
						fprintf(stdout, "lseek\n");
						off_t ret4 = handle_lseek(request + offset, request_len - offset);
						response = pack_lseek_response(opcode, ret4, &response_len);
						break;
					}
					case 5 : {
						fprintf(stdout, "stat\n");
						struct stat * stat_buf = malloc(sizeof(struct stat));
						int ret5 = handle_xstat(request + offset, request_len - offset, stat_buf);
						response = pack_xstat_response(opcode, ret5, &response_len, stat_buf);
						free(stat_buf);
						break;
					}
					case 6 : {
						fprintf(stdout, "unlink\n");
						int ret6 = handle_unlink(request + offset, request_len - offset);
						response = pack_unlink_response(opcode, ret6, &response_len);
						break;
					}
					case 7 : {
						fprintf(stdout, "getdirentries\n");
						char* content = malloc(*(int*) (request + offset + sizeof(int)));
						off_t* basep = malloc(sizeof(off_t));
						ssize_t ret7 = handle_getdirentries(request + offset, request_len - offset,content, basep);
						response = pack_getdirentries_response(opcode, ret7, &response_len, content, basep);
						free(content);
						break;
					}
					case 8: {
						fprintf(stdout, "getdirtree\n");
						struct dirtreenode* tree = handle_getdirtree(request + offset, request_len - offset);
						int size = 0;
						int tree_len = 0;
						if (tree != NULL) {
							get_size(tree, &tree_len);
							char* body = malloc(tree_len);
							encode_body(tree, body, &size);
							response = pack_getdirtree_response(opcode, body, &response_len, size);
							free(body);
						}
						freedirtree(tree);
						break;
					}
					default :
						free(response);
						break;
				}
				request_len = -1;
				opcode = -1;
                offset = 0;
				recv_len = 0;
				free(request);
				int send_length = 0;
				while (response_len > 0) {
					int sv = send(sessfd, response + send_length, response_len, 0);
					response_len -= sv;
					send_length += sv;

				}
				free(response);
			}
			close(sessfd);
			exit(0);

		}
		// either client closed connection, or error
		if (rv<0) err(1,0);
	}
	
	close(sockfd);
	free(buf);

	return 0;
}
/*
*pack the open and close response
*/
char* pack_int_response(int opcode, int ret, size_t *response_len) {
	int offset = sizeof(size_t);
	*response_len = 3 * sizeof(int) + sizeof(size_t);
	char* response = malloc(*response_len);
	memcpy(response, response_len, sizeof(size_t));
	memcpy(response + offset, &ret, sizeof(int));
	offset += sizeof(int);
	memcpy(response + offset, &opcode, sizeof(int));
	offset += sizeof(int);
	memcpy(response + offset, &errno, sizeof(int));
	return response;
}

/*
*pack the write response:
*First Pack the resonse length, then the return value 
*Then the opcode, errno, and finally the content if needed
*/
char* pack_write_response(int opcode, ssize_t ret, size_t *response_len) {
	int offset = sizeof(size_t);
	*response_len = 2 * sizeof(int) + sizeof(size_t)+ sizeof(ssize_t);
	char* response = malloc(*response_len);
	memcpy(response, response_len, sizeof(size_t));
	memcpy(response + offset, &ret, sizeof(ssize_t));
	offset += sizeof(ssize_t);
	memcpy(response + offset, &opcode, sizeof(int));
	offset += sizeof(int);
	memcpy(response + offset, &errno, sizeof(int));
	return response;
}

/*
*pack the read response
*First Pack the resonse length, then the return value 
*Then the opcode, errno, and finally the content if needed
*/
char* pack_read_response(int opcode, ssize_t ret3, size_t *response_len, char* content) {
	int offset = sizeof(size_t);
	*response_len = 2 * sizeof(int) + sizeof(size_t) + sizeof(ssize_t) + ret3;
	char* response = malloc(*response_len);
	memcpy(response, response_len, sizeof(size_t));
	memcpy(response + offset, &ret3, sizeof(ssize_t));
	offset += sizeof(ssize_t);
	memcpy(response + offset, &opcode, sizeof(int));
	offset += sizeof(int);
	memcpy(response + offset, &errno, sizeof(int));
	offset += sizeof(int);
	memcpy(response + offset, content, ret3);
	return response;

}

/*
*pack the lseek response
*First Pack the resonse length, then the return value 
*Then the opcode, errno, and finally the content if needed
*/
char* pack_lseek_response(int opcode, off_t ret, size_t *response_len){
	int offset = sizeof(size_t);
	*response_len = 2 *sizeof(int) + sizeof(size_t) + sizeof(off_t);
	char* response = (char*)malloc(*response_len);
	memcpy(response, response_len, sizeof(size_t));
	memcpy(response + offset, &ret, sizeof(off_t));
	offset += sizeof(off_t);
	memcpy(response + offset, &opcode, sizeof(int));
	offset += sizeof(int);
	memcpy(response + offset, &errno, sizeof(int));
	return response;
}

/*
*pack the stat response
*First Pack the resonse length, then the return value 
*Then the opcode, errno, and finally the content if needed
*/
char* pack_xstat_response(int opcode, int ret5, size_t *response_len, struct stat * stat_buf) {
	int offset = sizeof(size_t);
	*response_len = 3*sizeof(int)+sizeof(size_t) + sizeof(struct stat);
	char* response = malloc(*response_len);
	memcpy(response, response_len, sizeof(size_t));
	memcpy(response + offset, &ret5, sizeof(int));
	offset += sizeof(int);
	memcpy(response + offset, &opcode, sizeof(int));
	offset += sizeof(int);
	memcpy(response + offset, &errno, sizeof(int));
	offset += sizeof(int);
	memcpy(response + offset, stat_buf, sizeof(struct stat));
	return response;
}
/*
*pack the unlink response
*First Pack the resonse length, then the return value 
*Then the opcode, errno, and finally the content if needed
*/
char* pack_unlink_response(int opcode, int ret6, size_t *response_len) {
	int offset = sizeof(size_t);
	*response_len = 3 * sizeof(int) + sizeof(size_t);
	char* response = malloc(*response_len);
	memcpy(response, response_len, sizeof(size_t));
	memcpy(response + offset, &ret6, sizeof(int));
	offset += sizeof(int);
	memcpy(response + offset, &opcode, sizeof(int));
	offset += sizeof(int);
	memcpy(response + offset, &errno, sizeof(int));
	return response;
}

/*
*pack the getdirentries response
*First Pack the resonse length, then the return value 
*Then the opcode, errno, and finally the content if needed
*/
char* pack_getdirentries_response(int opcode, ssize_t ret7, size_t *response_len, char* content, off_t *basep) {
	int offset = sizeof(size_t);
	*response_len = 2*sizeof(int) + sizeof(size_t) + sizeof(ssize_t) + ret7 + sizeof(off_t);
	char* response = (char*) malloc(*response_len);
	memcpy(response, response_len, sizeof(size_t));
	memcpy(response + offset, &ret7, sizeof(ssize_t));
	offset += sizeof(ssize_t);
	memcpy(response + offset, &opcode, sizeof(int));
	offset += sizeof(int);
	memcpy(response + offset, &errno, sizeof(int));
	offset += sizeof(int);
	memcpy(response + offset, content, ret7);
	offset += ret7;

	memcpy(response + offset, basep, sizeof(off_t));
	free(basep);
	return response;
}

/*
*pack the getdirtree response
*First Pack the resonse length, then the return value 
*Then the opcode, errno, and finally the content if needed
*/
char* pack_getdirtree_response(int opcode, char* body, size_t* response_len, int size) {
	int offset = sizeof(size_t);
	*response_len = 2*sizeof(int) +sizeof(size_t)+ size;
	char* response = malloc(*response_len);
	memcpy(response, response_len, sizeof(size_t));
	memcpy(response + offset, &opcode, sizeof(int));
	offset += sizeof(int);
	memcpy(response + offset, &errno, sizeof(int));
	offset += sizeof(int);
	memcpy(response + offset, body, size);
	return response;
}

/*
*handle the open.
* unpack the request from the client 
* execute the operations
* get result.
*/
int handle_open(char* request, size_t request_len) {
	int offset = sizeof(int);
	int flags = *(int *) request;
	mode_t mode = *(mode_t *) (request + offset);
	offset += sizeof(mode_t);
	char* file = (char *)malloc(request_len - offset + 1);
	file[request_len - offset] = 0; // this is for the end of a file;
	memcpy(file, request+offset, request_len - offset);
	int ret = open(file, flags, mode);
	free(file);
	return ret;
}

/*
*handle the close.
* unpack the request from the client 
* execute the operations
* get result.
*/
int handle_close(char* request, size_t request_len) {
	int fd = *(int *) request;
	int ret = close(fd);
	return ret;
}

/*
*handle the write.
* unpack the request from the client 
* execute the operations
* get result.
*/
ssize_t handle_write(char* request, size_t request_len) {
	int offset = sizeof(int);
	int fd = *(int *) request;
	size_t count = *(size_t *) (request + offset);
	offset += sizeof(size_t);
	char* buf = malloc(count + 1);
	buf[count] = 0; // this is for the end of a file;
	memcpy(buf, request + offset, count);
	ssize_t ret = write(fd, buf, count);
	free(buf);
	return ret;
}

/*
*handle the read.
* unpack the request from the client 
* execute the operations
* get result.
*/
ssize_t handle_read(char* request, size_t request_len, char* buf) {
	int offset = sizeof(int);
	int fd = *(int *) request;
	size_t count = *(size_t *)(request + offset);
	ssize_t ret = read(fd, buf, count);
	return ret;
}

/*
*handle the lseek.
* unpack the request from the client 
* execute the operations
* get result.
*/
off_t handle_lseek(char* request, size_t request_len) {
	int offset = sizeof(int);
	int fd = *(int* )request;
	off_t off = *(off_t *) (request + offset);
	offset += sizeof(off_t);
	int whence = *(int* )(request + offset);
	off_t ret = lseek(fd, off, whence);
	return ret;
}
/*
*handle the xstat.
* unpack the request from the client 
* execute the operations
* get result.
*/
int handle_xstat(char* request, size_t request_len, struct stat * stat_buf) {
	int offset = sizeof(int);
	int ver = *(int*) (request);
	int len = *(int*) (request + offset);
	offset += sizeof(int);
	char* path = malloc(len + 1);
	path[len] = 0;
	memcpy(path, request + offset, len);
	int ret = __xstat(ver, path, stat_buf);
	free(path);
	return ret;
}
/*
*handle the unlink.
* unpack the request from the client 
* execute the operations
* get result.
*/
int handle_unlink(char* request, size_t request_len) {
	int offset = sizeof(int);
	int len = *(int* )(request);
	char* path = malloc(len + 1);
	path[len] = 0;
	memcpy(path, request + offset, len);
	int ret = unlink(path);
	free(path);
	return ret;
}

/*
*handle the getdirentries.
* unpack the request from the client 
* execute the operations
* get result.
*/
ssize_t handle_getdirentries(char* request, int request_len, char* content, off_t* basep) {
	int offset = sizeof(int);
	int fd = *(int *) request;
	size_t count = *(size_t *)(request + offset);
	offset += sizeof(size_t);
	basep = (off_t*)(request + offset);
	ssize_t ret = getdirentries(fd, content, count, basep);
	return ret;
}
/*
*handle the getdirtree.
* unpack the request from the client 
* execute the operations
* get result.
*/
struct dirtreenode* handle_getdirtree(char* request, int request_len) {
	char* path = malloc(request_len + 1);
	memcpy(path, request, request_len);
	path[request_len] = 0;
	struct dirtreenode* tree = getdirtree(path);
	free(path);
	return tree;
}

/*
*Get the size of the tree.
* unpack the request from the client 
* execute the operations
* get result.
*/
void get_size(struct dirtreenode* tree, int* size) {
	int len_name = strlen(tree->name);
	*size += sizeof(int);
	*size += len_name;
	*size += 1;
	int num = tree->num_subdirs;
	*size += sizeof(int);
	int i;
	for (i = 0; i < num; i++) {
		get_size(tree->subdirs[i], size);
	}
}
/*
*Encode the body.
* pack the result tree
* get result.
*/
void encode_body(struct dirtreenode* tree, char* body, int* size) {
	int len_name = strlen(tree->name);
	memcpy(body + *size, &len_name, sizeof(int));
	*size += sizeof(int);
	memcpy(body + *size, tree->name, len_name);
	*size += len_name;
	body[*size] = 0;
	*size += 1;
	int num = tree->num_subdirs;
	memcpy(body + *size, &num, sizeof(int));
	*size += sizeof(int);
	int i;
	for (i = 0; i < num; i++) {
		encode_body(tree->subdirs[i], body, size);
	}
}