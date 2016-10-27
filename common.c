/** 
  * Project Name:   Peer-to-peer File Sharing System
  * Name:  Khushubu Lohiya
  **/

#include "common.h"

pthread_mutex_t print_lock = PTHREAD_MUTEX_INITIALIZER;

//================================================================================================================

int connect_to_server(char *address, uint32_t port_param)
{

	struct sockaddr_in server;
	struct hostent *server_at;

	//Create TCP socket for communication with server
	int client_socket = socket(AF_INET, SOCK_STREAM, 0);
	if(client_socket == -1)
	{
		printf("Error creating socket.\n");
		return -1;
	}

	memset(&server, 0, sizeof(server));
	int port = port_param; //atoi(port_param);
	server.sin_family = AF_INET;

	server_at = gethostbyname(address);
	bcopy((char*)server_at -> h_addr, (char*)&server.sin_addr.s_addr, server_at -> h_length);
	server.sin_port = htons(port);

	//Send connection request to the server
	if(connect(client_socket, (struct sockaddr *)&server, sizeof(server)) < 0)
	{
		printf("Connection failed.\n");
		return -1;
	}

	return client_socket;
}

//================================================================================================================

//*****************************************************************************************************************************
// for reading server information 
//****************************************************************************************************************************

ServerInfo *read_server_info(int server_socket)
{
	ServerInfo* server_info = (ServerInfo*)malloc(sizeof(ServerInfo));
	
	uint32_t server_name_length;
	read(server_socket, &server_name_length, sizeof(server_name_length));
	server_name_length = ntohl(server_name_length);
	
	bzero(server_info -> server_name, server_name_length + 1);
	read(server_socket, server_info -> server_name, server_name_length);
	
	uint32_t address_length;
	read(server_socket, &address_length, sizeof(address_length));
	address_length = ntohl(address_length);
	
	bzero(server_info -> server_ip, address_length + 1);
	read(server_socket, server_info -> server_ip, address_length);

	read(server_socket, &server_info -> server_port, sizeof(server_info -> server_port));
	server_info -> server_port = ntohl(server_info -> server_port);

	printf("Received server info server name=%s server ip=%s server port=%d\n",
			server_info -> server_name, server_info -> server_ip, server_info ->server_port);
	return server_info;
}

//================================================================================================================
/**
 * Receive info as a byte stream 
 */
//================================================================================================================

BlobInfo create_file_info_blob(FileInfo file_info)
{

	uint32_t totalsize = 4 + strlen(file_info.filename) + 4 + SHA_DIGEST_LENGTH;
	printf("total size %u\n", totalsize);
	char* blob = (char*) malloc(totalsize);
	memset(blob, 0, totalsize);
	char* blobPtr = blob;

	uint32_t filenamelength = strlen(file_info.filename);
	uint32_t fileNameLength_NW = htonl(filenamelength);
	memcpy(blob, &fileNameLength_NW, 4);
	blob = blob + 4;

	memcpy(blob, file_info.filename, filenamelength);
	blob = blob + filenamelength;

	uint32_t filesize_network = htonl(file_info.filesize);
	memcpy(blob, &filesize_network, 4);
	blob = blob + 4;

	memcpy(blob, file_info.hash, SHA_DIGEST_LENGTH);

	BlobInfo blob_info;
	blob_info.blob = blobPtr;
	blob_info.bloblength = totalsize;

	return blob_info;
}

//================================================================================================================
/**
 * For converting byte stream to message in readable format
 */
//================================================================================================================

FileInfo parse_file_info_blob(char* blob)
{
	FileInfo file_info;

	uint32_t filename_length;
	memcpy(&filename_length, blob, 4);
	filename_length = ntohl(filename_length);
	blob = blob + 4;

	file_info.filename = (char*)malloc(filename_length + 1);
	memset(file_info.filename, 0, filename_length + 1);
	memcpy(file_info.filename, blob, filename_length);
	blob = blob + filename_length;

	memcpy(&(file_info.filesize), blob, 4);
	file_info.filesize = ntohl(file_info.filesize);
	blob = blob + 4;

	memcpy(file_info.hash, blob, SHA_DIGEST_LENGTH);

	print_file_info(file_info);
	return file_info;
}

//================================================================================================================
/**
 * For printing the file information
 */
//================================================================================================================

void print_file_info(FileInfo file_info)
{
	pthread_mutex_lock(&print_lock);
	printf("\nFileName: %s\n", file_info.filename);
	printf("FileSize: %u Bytes\n", file_info.filesize);
	printf("SHA1: ");
	for (int i= 0; i < SHA_DIGEST_LENGTH; i++)
	{
		printf("%02x", file_info.hash[i]);
	}
	printf("\n");
	pthread_mutex_unlock(&print_lock);
}

//================================================================================================================
// For sending the file to the client
//================================================================================================================

void send_file_to_client(int socket, FileInfo file_info)
{
	FILE* fp = fopen(file_info.filename, "r");
	if (fp == NULL)
	{
		printf("No such file %s, skipping\n", file_info.filename);
		// send error message to socket
		return;
	}

	BlobInfo blob_info = create_file_info_blob(file_info);
	uint32_t blob_length_nw = htonl(blob_info.bloblength);

	//printf("blob length in write to %d is network=%u host=%u\n", serving_socket, blob_length_nw, blob_info.bloblength);

	write(socket, &blob_length_nw, sizeof(blob_info.bloblength));
	write(socket, blob_info.blob, blob_info.bloblength);

	while (1) {

		char buffer[CHUNK_SIZE];
		int nread = fread(buffer, 1, CHUNK_SIZE, fp);
		//printf("Bytes read from file %d \n", nread);

		if(nread > 0)
		{
			write(socket, buffer, nread);
		}
		else {
			if (feof(fp))
			{
				printf("End of file\n");
			}
			else if (ferror(fp))
			{
				printf("Error reading\n");
			}
			break;
		}
	}

	printf("File closed\n");
	fclose(fp);
}

//================================================================================================================
// For checking if SHA1 is same
//================================================================================================================

bool is_sha1_same(unsigned char* hash1, unsigned char* hash2)
{
	for(int i = 0; i < SHA_DIGEST_LENGTH; i++)
	{
		if (hash1[i] != hash2[i])
		{
			return False;
		}
	}
	return True;
}

//================================================================================================================
//For getting the file from client
//================================================================================================================

FileInfo* get_file_from_client(int socket, int *err)
{
	uint32_t blob_length_nw;
	int read_count = read(socket, &blob_length_nw, sizeof(blob_length_nw));
	if (read_count <= 0)
	{
		printf("Error reading from socket in get_file_from_client.\n");
		*err = -1;
		return NULL;
	}
	uint32_t blob_length = ntohl(blob_length_nw);

	char* blob = (char *) malloc(blob_length);
	read_count = read(socket, blob, blob_length);
	if (read_count <= 0)
	{
		printf("Error reading from get_file_from_client socket.\n");
		return NULL;
	}

	FileInfo file = parse_file_info_blob(blob);

	// now open file in "w" mode which creates a new file
	FILE *fp;
	fp = fopen(file.filename, "w");

	if(NULL == fp)
	{
		printf("Could not create file, exiting");
		exit(-1);
	}
	else
	{
		printf("File %s created\n", file.filename);
	}

	int incoming_file_bytes = 0;
	SHA_CTX ctx;
	SHA1_Init(&ctx);

	while (incoming_file_bytes < file.filesize) {

		char buffer[CHUNK_SIZE];
		// read from socket
		int bytes = read(socket, buffer, CHUNK_SIZE);
		if (bytes > 0)
		{
			// update SHA
			SHA1_Update(&ctx, buffer, bytes);
			fwrite(buffer, 1, bytes, fp);
			// keep adding the incoming bytes to keep track of number of bytes read from client
			incoming_file_bytes = incoming_file_bytes + bytes;
		}
		else { // this can happen if client dies and closes socket when server is waiting on read

			// if bytes is less than 0, it means the client went off before sending us the entire file
			printf("Incomplete file received\n");
			fclose(fp);
			return NULL;
		}
	}

	if (incoming_file_bytes == file.filesize)
	{
		// final SHA
		unsigned char hash[SHA_DIGEST_LENGTH];
		SHA1_Final(hash, &ctx);
		// compare final SHA with file.hash
		// if same then only do below lines, else print error message
		if(is_sha1_same(file.hash, hash) == False)
		{
			printf("SHA1 mismatch, ignoring file received from client\n");
			fclose(fp);
			return NULL;
		}
	} else
	{
		fclose(fp);
		return NULL;
	}
	fclose(fp);

	// allocate heap memory to return file info
	FileInfo *file_info_to_be_returned = (FileInfo*) malloc(sizeof(FileInfo));

	// as filename is char pointer, we allocate memory first
	file_info_to_be_returned -> filename = (char *)malloc(strlen(file.filename + 1));
	bzero(file_info_to_be_returned -> filename, strlen((file.filename + 1)));
	strcpy(file_info_to_be_returned -> filename, file.filename);

	file_info_to_be_returned -> filesize = file.filesize;

	for (int i = 0; i < SHA_DIGEST_LENGTH; i++)
	{
		file_info_to_be_returned -> hash[i] = file.hash[i];
	}
	return file_info_to_be_returned;
}


//================================================================================================================
// 1731 getting file lists from server
//================================================================================================================


void get_file_list_from_server(int socket, FileInfo *file_list_from_server, uint32_t *list_of_files_count)
{
	// receive response
	uint32_t file_count;
	read(socket, &file_count, sizeof(file_count));
	file_count = ntohl(file_count);

	if (file_count == 0)
	{
		printf("No files available with server for download!!\n");
	}
	else {
		printf("Received file count %d from server %d\n", file_count, socket);
		for (int i = 0; i < file_count; i++)
		{
			uint32_t blob_length;
			read(socket, &blob_length, sizeof(blob_length));
			blob_length = ntohl(blob_length);

			char *blob = (char *)malloc(blob_length);
			read(socket, blob, blob_length);

			FileInfo file_info = parse_file_info_blob(blob);
			file_list_from_server[i] = file_info;
		}
	}
	*list_of_files_count = file_count;
}


//================================================================================================================
// Queue Code
//================================================================================================================
// For printing the file info
//================================================================================================================

void print_queue(FileInfoNode *front, FileInfoNode *rear)
{
	FileInfoNode *temp = front;
	if(temp == NULL)
	{
		printf("Queue is empty.\n");
		return;
	}
	printf("Data in queue is: \n");
	printf(" HEAD => ");
	while(temp != NULL)
	{
		printf("(%s, %d) -> ", temp -> file_info.filename, temp -> file_info.filesize);
		temp = temp -> next;
	}
	printf(" <= TAIL\n");
}

//================================================================================================================
// Check whether the queue is empty
//================================================================================================================

bool is_queue_empty(FileInfoNode *front, FileInfoNode *rear)
{
	print_queue(front, rear);
	if (front == NULL && rear == NULL)
	{
		return True;
	}
	return False;
}

//================================================================================================================
// To enqueue the file receive
//================================================================================================================

void enqueue(FileInfoNode **front, FileInfoNode **rear, FileInfo file_info)
{
	FileInfoNode *temp = (FileInfoNode*)malloc(sizeof(FileInfoNode));
	temp -> next = NULL;
	temp -> file_info.filename = file_info.filename;
	temp -> file_info.filesize = file_info.filesize;
	for (int i = 0; i < SHA_DIGEST_LENGTH; i++)
	{
		temp -> file_info.hash[i] = file_info.hash[i];
	}

	if(*front == NULL && *rear == NULL)
	{
		*front = temp;
		*rear = temp;
	}
	else
	{
		(*rear) -> next = temp;
		*rear = temp;
	}
	print_queue(*front, *rear);
	return;
}

//================================================================================================================
// To dequeue the file
//================================================================================================================

FileInfoNode *dequeue(FileInfoNode **front, FileInfoNode **rear)
{
	FileInfoNode *temp = *front;
	if(*front == NULL)
	{
		printf("No data to dequeue. Queue is empty.\n");
		return NULL;
	}
	if(*front == *rear)
	{
		*front = NULL;
		*rear = NULL;
	}
	else
	{
		*front = (*front) -> next;
	}
	print_queue(*front, *rear);
	return temp;
}

//================================================================================================================
