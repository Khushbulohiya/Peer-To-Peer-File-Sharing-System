/** 
  * Project Name:   Peer-to-peer File Sharing System
  * Name:  Khushubu Lohiya
 **/

#include "headers.h"

uint8_t I_AM_SERVER_HELLO = 0x01;
uint8_t I_AM_CLIENT_HELLO = 0x02;

// Message type for client and server communication
uint8_t M_ID_UPLOAD_FILES   = 0x03;
uint8_t M_ID_LIST_FILES     = 0x04;
uint8_t M_ID_SEARCH         = 0x05;
uint8_t M_ID_DOWNLOAD_FILES = 0x06;
uint8_t M_ID_DISCONNECT     = 0x07;

/**
 * New Messages for the new additional feature
 */
uint8_t M_ID_GET_PEER_SERVER_INFO_REQ = 0x08;
uint8_t M_ID_GET_PEER_SERVER_INFO_RES = 0x09;
uint8_t M_ID_GET_PEER_SERVER_INFO_NO_SERVER = 0x10;

/**
 * New header server information
 */
char *head_server_address="localhost";
int head_server_port = 5001;