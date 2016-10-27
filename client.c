/** 
  * Project Name:   Peer-to-peer File Sharing System
  * Name:  Khushubu Lohiya
 **/

#include "serverclient.h"
#include "common.h"

extern ServerInfo *read_server_info(int);
extern FileInfo* get_file_from_client(int, int *);
extern void send_file_to_client(int, FileInfo);
extern void get_file_list_from_server(int, FileInfo *, uint32_t *);
extern int connect_to_server(char *, uint32_t);

FileInfo list_of_files[MAX];
uint32_t list_of_files_count = 0;

FileInfo queried_files[MAX];
uint32_t queried_files_count = 0;

char *config_file_name = "servers_config.txt";
ServerInfo servers[MAX_SERVERS];
int total_server_count = 0;

//================================================================================================================
// for initializing server configuration
//===============================================================================================================

void initialize_server_config()
{
	FILE *fp;
	fp = fopen(config_file_name, "r");
	if(fp == NULL)
	{
		printf("Error opening file, No such file exists.\n");
		exit(-1);
	}
	int i = 0;
	while (1)
	{

	    int ret = fscanf(fp, "%s %s %u", servers[i].server_name, servers[i].server_ip, &servers[i].server_port);

		if (ret <= 0)
		{
		   break;
		}
		i = i + 1;
		if (fgetc(fp) == EOF)
		{
			break;
		}
	}
	total_server_count = i;
	fclose(fp) ;
}

//================================================================================================================
// for getting the server configuration
//================================================================================================================

ServerInfo* get_my_config(char *server_name)
{
	for (int i = 0; i < total_server_count; i++)
	{
		if (strcmp(servers[i].server_name, server_name) == 0)
		{
			return &(servers[i]);
		}

	}
	return NULL;
}

//================================================================================================================

int display()
{
	int enter_id;
	printf("\t--------MENU---------\n");
	printf("1.\tList all the files.\n");
	printf("2.\tDownload file from the server.\n");
	printf("3.\tUpload file to the server.\n");
	printf("4.\tDisconnect the connection.\n");
	printf("Enter the input id: ");
	scanf("%d",&enter_id);
	return enter_id;
}


//================================================================================================================

void send_message_id_to_server(int socket, uint8_t message_id)
{
	//printf("*** Write message type %d to client %d\n\n\n", message_id, socket);
	write(socket, &message_id, sizeof(message_id));
}

//================================================================================================================
// for displaying file lists
//================================================================================================================

void display_file_list(FileInfo *file_list, int file_count)
{
	if (file_count > 0)
	{
		printf("\n\nList of files received from server below:\n");

		printf("#  File Name \t File Size\n");
		for (int i = 0; i < file_count; i++)
		{
			printf("%d. %s \t %d Bytes\n", i, file_list[i].filename, file_list[i].filesize);
		}
		printf("\n\n");
	}
	return;
}

//================================================================================================================
// sending file lists request
//================================================================================================================

void send_file_list_request(int socket)
{
	// send request
	send_message_id_to_server(socket, M_ID_LIST_FILES);
	get_file_list_from_server(socket, list_of_files, &list_of_files_count);
	return;
}

//================================================================================================================
// search file with query
//================================================================================================================

void search_on_server(int socket)
{
	char query[MAX];
	printf("Please enter search query (one word please): ");
	scanf("%s", query);

	// send message id to server to represent its search
	send_message_id_to_server(socket, M_ID_SEARCH);

	// write query length
	uint32_t query_length_nw = htonl(strlen(query));
	write(socket, &query_length_nw, sizeof(query_length_nw));

	// send the query term to the server
	write(socket, query, strlen(query));

	// now wait for the list of files
	get_file_list_from_server(socket, queried_files, &queried_files_count);
}

//================================================================================================================
//  search file by name for download it from server
//================================================================================================================
char *get_filename(FileInfo *file_list, int count)
{
	display_file_list(file_list, count);

	if (count == 0)
	{
		printf("There is no file for download.\n");
		return NULL;
	}

	printf("\n Please enter the file number you want to download: ");
	int input;
	scanf("%d",&input);
	if(input >= count || input < 0)
	{
		printf("Invalid file id.\n");
		return NULL;
	}
	return file_list[input].filename;
}

//================================================================================================================


void download_file_from_server(int socket)
{
	int input;
	printf("\t\t-----Download Menu-----\n\n");
	printf("\t1. Get all files from server\n");
	printf("\t2. Search for file\n");
	printf("\nPlease enter your choice: ");
	scanf("%d", &input);

	char* filename;
	switch(input) {
		case 1: // get all files and download
			send_file_list_request(socket);
			filename = get_filename(list_of_files, list_of_files_count);
			break;

		case 2: // get queried files and then download
			search_on_server(socket);
			filename = get_filename(queried_files, queried_files_count);
			break;

		default:
			printf("Invalid input\n");
			return;
	}

	if (filename == NULL)
	{
		printf("No file found to download from server, Sorry!!\n");
		return;
	}

	//Sending message request to server
	send_message_id_to_server(socket, M_ID_DOWNLOAD_FILES);

	uint32_t filename_length = strlen(filename);
	filename_length = htonl(filename_length);
	write(socket, &filename_length, sizeof(filename_length));

	write(socket, filename, strlen(filename));

	int err;

	// now get the file from server
	FileInfo *file = get_file_from_client(socket, &err);

	if (file == NULL)
	{
		printf("Error Opening file\n");
	}

}

//================================================================================================================
// for getting SHA1 of the file
//================================================================================================================
bool get_sha1(char * filename, unsigned char *hash)
{
	FILE *fp;
	fp = fopen(filename,"r");
	if(fp == NULL)
	{

		printf("Error opening file");
		return False;
	}
	SHA_CTX ctx;
	SHA1_Init(&ctx);

	char buffer[1024];
	while (1) {

		int nread = fread(buffer, 1, 20, fp);
		if(nread > 0)
		{
			SHA1_Update(&ctx, buffer, nread);
		}
		else {
			if (feof(fp))
			{
				SHA1_Final(hash, &ctx);
				printf("SHA1:");
				for (int i= 0; i< SHA_DIGEST_LENGTH; i++)
				{
					printf("%02x", hash[i]);
				}
				printf("\n");
			}
			else if (ferror(fp))
			{
				return False;
			}
			break;
		}
	}
	return True;
}

//================================================================================================================
// for uploading file to server
//================================================================================================================


void upload_file_to_server(int socket)
{
	printf("Enter the name of the file you want to upload: ");
	char filename[MAX];
	scanf("%s", filename);

	struct stat st;
	stat(filename, &st);

	FileInfo file_info;
	file_info.filename = filename;
	file_info.filesize = st.st_size;
	if (get_sha1(filename, file_info.hash) == False)
	{
		printf("Error calculating SHA1 of file %s\n", filename);
		return;
	}
	send_message_id_to_server(socket, M_ID_UPLOAD_FILES);
	send_file_to_client(socket, file_info);
}

//================================================================================================================

//*****************************************************************************************************************************
// To get server configuration frm head server server information 
//*****************************************************************************************************************************

ServerInfo* get_server_config()
{
	int server_socket = connect_to_server(head_server_address, head_server_port);
	if (server_socket == -1)
	{
		printf("Error connecting to server. \n Exiting..!!\n");
		return NULL;
	}
	printf("Connected to head server %s:%d\n", head_server_address, head_server_port);
	send_message_id_to_server(server_socket, M_ID_GET_PEER_SERVER_INFO_REQ);
	printf("Sending request to head server for server info with message id %d\n", M_ID_GET_PEER_SERVER_INFO_REQ);
	
	uint8_t message_id_response;
	read(server_socket, &message_id_response, sizeof(message_id_response));
	printf("Received response message id %d from head server\n", message_id_response);
	if (M_ID_GET_PEER_SERVER_INFO_RES == message_id_response)
	{
		return read_server_info(server_socket);
	}
	else 
	{
		printf("Error, received error response from server %d\n", message_id_response);
	}
	return NULL;
}

//======================================================================================================================

int main(int argc, char *argv[])
{
	if(argc != 1)
	{
		fprintf(stderr, "usage %s\n", argv[0]);
		return -1;
	}

	ServerInfo* my_info = get_server_config();

	if (my_info == NULL) 
	{
		printf("No server config received from head server, exiting!! :(\n");
		exit(-1);
	}

	printf("Received server info from head server name=%s ip=%s port=%d\n", 
	   		my_info -> server_name, my_info -> server_ip, my_info -> server_port);
	printf("Connected to server %s\n",my_info -> server_name);
	
	int server_socket = connect_to_server(my_info -> server_ip, my_info -> server_port);
	//After connection inform server that I am client
	write(server_socket, &I_AM_CLIENT_HELLO, sizeof(I_AM_CLIENT_HELLO));
	
	if (server_socket == -1)
	{
		printf("Error connecting to server. \n Exiting..!!\n");
		return -1;
	}
	while(1)
	{
		int input = display();
		switch(input)
		{
			case 1: send_file_list_request(server_socket);
					display_file_list(list_of_files, list_of_files_count);
				break;

			case 2: download_file_from_server(server_socket);
				break;

			case 3: upload_file_to_server(server_socket);
				break;

			case 4:
				send_message_id_to_server(server_socket, M_ID_DISCONNECT);
				printf("Requested server to close the socket. Waiting for 1 seconds \n");
				sleep(1);
				close(server_socket);
				return -1;

			default: printf("Incorrect selection. Please select valid choice.\n");
				break;
		}
	}
}

//================================================================================================================

