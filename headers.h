/** 
  * Project Name:   Peer-to-peer File Sharing System
  * Name:  Khushubu Lohiya
 **/
  
#include<stdio.h>
#include<stdlib.h>
#include<string.h>
#include<strings.h>
#include<unistd.h>
#include<sys/stat.h>
#include<sys/socket.h>
#include<sys/types.h>
#include<arpa/inet.h>
#include<errno.h>
#include<time.h>
#include<pthread.h>
#include<openssl/sha.h>
#include<netdb.h>
#include<netinet/in.h>
#include<signal.h>

#ifndef HEADERS_INIT

#define HEADERS_INIT 1

#define h_addr h_addr_list[0]
#define bool uint8_t
#define True 1
#define False 0
#define CHUNK_SIZE 20
#define MAX 500
#define MAX_SERVERS 100
#define MAX_FILES 500

#endif
