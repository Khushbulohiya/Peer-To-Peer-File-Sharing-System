/** 
  * Project Name:   Peer-to-peer File Sharing System
  * Name:  Khushubu Lohiya
 **/

#include "headers.h"

//============================================================
typedef struct ServerPeerInfoStruct {
	char* server_name;
	int socket;
	bool connected;
	// queue front
	FileInfoNode * front;
	// queue rear
	FileInfoNode * rear;
	// queue lock
	pthread_mutex_t server_peer_writer_queue_lock;
	// queue condition variable
	pthread_cond_t server_peer_writer_queue_cv;
	bool get_files;
} ServerPeerInfo;

//============================================================

FileInfo file_info_list[MAX_FILES];
uint32_t file_info_count = 0;
pthread_mutex_t file_info_list_lock;

//============================================================
ServerPeerInfo server_peer_info[MAX_SERVERS];
int connected_server_count = -1;
pthread_mutex_t server_peer_info_lock;

//============================================================
