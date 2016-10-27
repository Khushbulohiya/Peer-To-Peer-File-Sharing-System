/** 
  * Project Name:   Peer-to-peer File Sharing System
  * Name:  Khushubu Lohiya
 **/

#include "headers.h"

//=====================================================================

typedef struct FileInfoStruct
{
	char *filename;
	uint32_t filesize;
	unsigned char hash[SHA_DIGEST_LENGTH];
} FileInfo;

//=====================================================================

typedef struct BlobInfoStruct
{
	char *blob;
	uint32_t bloblength;
} BlobInfo;

//=====================================================================

typedef struct QueueInfoNode {
	FileInfo file_info;
	struct QueueInfoNode *next;
} FileInfoNode;

//=====================================================================

typedef struct ServerInfoStruct {
	char server_name[40];
	char server_ip[30];
	uint32_t server_port;
} ServerInfo;

//======================================================================
// function declarations
//======================================================================

ServerInfo *read_server_info(int);
int connect_to_server(char *, uint32_t);
void print_file_info(FileInfo file_info);
BlobInfo create_file_info_blob(FileInfo file_info);
FileInfo parse_file_info_blob(char* blob);
void send_file_to_client(int socket, FileInfo file_info);
bool is_sha1_same(unsigned char* hash1, unsigned char* hash2);
FileInfo* get_file_from_client(int socket, int *err);
void get_file_list_from_server(int socket, FileInfo *file_list_from_server, uint32_t *list_of_files_count);

void print_queue(FileInfoNode *front, FileInfoNode *rear);
bool is_queue_empty(FileInfoNode *front, FileInfoNode *rear);
void enqueue(FileInfoNode **front, FileInfoNode **rear, FileInfo file_info);
FileInfoNode *dequeue(FileInfoNode **front, FileInfoNode **rear);
