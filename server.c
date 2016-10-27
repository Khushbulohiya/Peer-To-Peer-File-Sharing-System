/** 
  * Project Name:   Peer-to-peer File Sharing System
  * Name:  Khushubu Lohiya
 **/

#include "serverclient.h"
#include "common.h"
#include "server.h"

#define MAX_READER_THREADS 100
#define MAX_WRITER_THREADS 100

extern ServerInfo *read_server_info(int);
extern int connect_to_server(char *, uint32_t);
extern FileInfo* get_file_from_client(int, int *);
extern void send_file_to_client(int, FileInfo);
extern bool is_queue_empty(FileInfoNode *front, FileInfoNode *rear);
extern void enqueue(FileInfoNode **front, FileInfoNode **rear, FileInfo file_info);
extern FileInfoNode *dequeue(FileInfoNode **front, FileInfoNode **rear);
extern void get_file_list_from_server(int, FileInfo *, uint32_t *);

int fail = -1;
char *server_name;
bool flag = True;

int server_acceptor_socket;

int accepted_sockets[MAX];
int accepted_sockets_count = 0;

int connected_to_sockets[MAX];
int connected_to_sockets_count = 0;


typedef struct SocketInfoStruct {
	int socket;
	bool is_client;
}SocketInfo;

char *config_file_name = "servers_config.txt";
ServerInfo servers[MAX_SERVERS];
int total_server_count = 0;

int server_index = 0;

ServerInfo* my_info;

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
	fclose(fp);
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
//for graceful shutdown
//================================================================================================================

void signal_handler(int sig_type)
{
	char  c;
	signal(sig_type, SIG_IGN);
	printf("Do you really want to quit? [y/n] ");

	c = getchar();
	if (c == 'y' || c == 'Y')
	{

		flag = False;

		shutdown(server_acceptor_socket, SHUT_RDWR);
		close(server_acceptor_socket);

		for (int i = 0; i < accepted_sockets_count; i++)
		{
			shutdown(accepted_sockets[i], SHUT_RDWR);
			close(accepted_sockets[i]);
		}

		for (int i = 0; i < connected_to_sockets_count; i++)
		{
			shutdown(connected_to_sockets[i], SHUT_RDWR);
			close(connected_to_sockets[i]);
		}

		for (int i = 0; i <= connected_server_count; i++)
		{
			// for the server writer thread, we take lock and signal
			pthread_mutex_lock(&server_peer_info[i].server_peer_writer_queue_lock);

			pthread_cond_signal(&server_peer_info[i].server_peer_writer_queue_cv);
			// release the writer queue lock
			pthread_mutex_unlock(&server_peer_info[i].server_peer_writer_queue_lock);
		}
	}
	else
	{
		signal(SIGINT, signal_handler);
	}
}

//================================================================================================================

//*****************************************************************************************************************************
// for sending server information 
//*****************************************************************************************************************************
void send_server_info(int socket, ServerInfo server_info)
{
	uint32_t server_name_length_nw = htonl(strlen(server_info.server_name));
	write(socket, &server_name_length_nw, sizeof(server_name_length_nw));

	write(socket, server_info.server_name, strlen(server_info.server_name));

	uint32_t address_length_nw = htonl(strlen(server_info.server_ip));
	write(socket, &address_length_nw, sizeof(address_length_nw));

	write(socket, server_info.server_ip, strlen(server_info.server_ip));

	uint32_t port_nw = htonl(server_info.server_port);
	write(socket, &port_nw, sizeof(port_nw));
	printf("server name=%s server ip=%s port=%d\n",
		   server_info.server_name, server_info.server_ip, server_info.server_port);

	return;
}

//================================================================================================================

void get_file_from_server(int server_socket)
{
	int err = 0;
	FileInfo *file = get_file_from_client(server_socket, &err);

	if (file != NULL)
	{
		printf("\nFile read from client complete, closing file.\n");
		pthread_mutex_lock(&file_info_list_lock);
		file_info_list[file_info_count] = *file;
		file_info_count++;
		pthread_mutex_unlock(&file_info_list_lock);
	}
	else
	{
		if (err == -1)
		{
			if (!flag)
			{
				printf("Control C pressed, so exiting.\n");
			}
			else
			{
				printf("Seems like peer server socket closed, so exiting\n");
			}
		}
		else
		{
			printf("Problem receiving file from server.\n");
		}
	}

}

//================================================================================================================
// for server sync
//================================================================================================================
void get_files_update(SocketInfo *server_peer_info)
{

	int serving_socket = server_peer_info -> socket;

	// write  M_ID_LIST_FILES
	printf("Sending list files request to other server %d, message_id=%d\n", serving_socket, M_ID_LIST_FILES);
	write(serving_socket, &M_ID_LIST_FILES, sizeof(M_ID_LIST_FILES));

	// send message for list of files
	FileInfo list_of_files[MAX];
	uint32_t list_of_files_count = 0;

	get_file_list_from_server(serving_socket, list_of_files, &list_of_files_count);

	// send message to download each file
	for (int i = 0; i < list_of_files_count; i++)
	{
		// for each download write M_ID_DOWNLOAD_FILES
		printf("Sending download request to download file %s from other server %d, message_id=%d\n",
			   list_of_files[i].filename, serving_socket, M_ID_DOWNLOAD_FILES);

		write(serving_socket, &M_ID_DOWNLOAD_FILES, sizeof(M_ID_DOWNLOAD_FILES));

		uint32_t filename_length = strlen(list_of_files[i].filename);
		filename_length = htonl(filename_length);
		write(serving_socket, &filename_length, sizeof(filename_length));

		write(serving_socket, list_of_files[i].filename, strlen(list_of_files[i].filename));

		int err;

		// now get the file from server
		FileInfo *file = get_file_from_client(serving_socket, &err);

		if (file == NULL)
		{
			printf("Error getting file %s from server %d\n", list_of_files[i].filename, serving_socket);
		}
		else
		{
			pthread_mutex_lock(&file_info_list_lock);

			// as filename is char pointer, we allocate memory first
			file_info_list[file_info_count].filename = (char *)malloc(strlen(file -> filename + 1));
			bzero(file_info_list[file_info_count].filename, strlen((file -> filename + 1)));
			strcpy(file_info_list[file_info_count].filename, file -> filename);

			file_info_list[file_info_count].filesize = file -> filesize;

			for (int i = 0; i < SHA_DIGEST_LENGTH; i++)
			{
				file_info_list[file_info_count].hash[i] = file -> hash[i];
			}
			free(file);
			file_info_count++;
			pthread_mutex_unlock(&file_info_list_lock);
			printf("Added file %s to the list of files served, and the total file count now is %d\n",
				   file -> filename, file_info_count);
		}
	}
}

//================================================================================================================
// for queue part
//================================================================================================================

void *write_to_server(void *server_peer_info_param)
{
	ServerPeerInfo * server_peer_info = (ServerPeerInfo *) server_peer_info_param;
	int serving_socket = server_peer_info -> socket;

	while (flag)
	{
		pthread_mutex_lock(&server_peer_info -> server_peer_writer_queue_lock);

		// only if flag is empty and queue is empty then we wait on the condition
		// else we go out of the loop and don't wait
		// flag = false, means control C was pressed
		while (flag && is_queue_empty(server_peer_info -> front, server_peer_info -> rear))
		{

			printf("Writer waiting for signal to write to server on socket %d\n", serving_socket);
			pthread_cond_wait(&server_peer_info -> server_peer_writer_queue_cv, &server_peer_info -> server_peer_writer_queue_lock);
		}

		// if control C is pressed, we break from the loop and exit from the thread
		// we don't want to wait in this pthread any more
		if (!flag)
		{
			printf("Exiting write to server thread, control C pressed or socket closed\n");
			break;
		}

		FileInfoNode* node = dequeue(&server_peer_info -> front, &server_peer_info -> rear);
		pthread_mutex_unlock(&server_peer_info -> server_peer_writer_queue_lock);
		printf("Received signal from reader about new file, dequeued from queue name=%s size=%d\n", node->file_info.filename, node->file_info.filesize);

		// before sending to client send the message id
		// write M_ID_UPLOAD_FILES
		printf("Sending the new file info and file %s to server socket %d, message_id=%d\n",
			   node->file_info.filename, serving_socket, M_ID_UPLOAD_FILES);
		write(serving_socket, &M_ID_UPLOAD_FILES, sizeof(M_ID_UPLOAD_FILES));
		send_file_to_client(serving_socket, node -> file_info);
	}

	pthread_exit(&fail);
}

//================================================================================================================
// for sending file lists to client
//================================================================================================================

void send_file_list_to_client(int client_socket)
{

	pthread_mutex_lock(&file_info_list_lock);

	uint32_t file_count_nw = htonl(file_info_count);
	write(client_socket, &file_count_nw, sizeof(file_count_nw));

	for (int i = 0; i < file_info_count; i++)
	{
		FileInfo file_info = file_info_list[i];
		BlobInfo blob_info = create_file_info_blob(file_info);

		uint32_t blob_length_nw = htonl(blob_info.bloblength);
		write(client_socket, &blob_length_nw, sizeof(blob_info.bloblength));
		write(client_socket, blob_info.blob, blob_info.bloblength);

	}
	pthread_mutex_unlock(&file_info_list_lock);

}

//================================================================================================================
// for sending file lists to client
//================================================================================================================

int get_file_info_from_filename(char *filename)
{

	pthread_mutex_lock(&file_info_list_lock);

	for (int i = 0; i < file_info_count; i++)
	{
		FileInfo file_info = file_info_list[i];
		if (strcmp(file_info.filename, filename) == 0)
		{
			pthread_mutex_unlock(&file_info_list_lock);
			printf("File found at index %d\n", i);
			return i;
		}
	}
	pthread_mutex_unlock(&file_info_list_lock);
	return -1;
}

//================================================================================================================
// for getting file from client
//================================================================================================================

void get_file_uploaded_by_client(int client_socket, bool is_client)
{
	// get the file from client that is uploaded
	int err;
	FileInfo *file = get_file_from_client(client_socket, &err);
	if (file != NULL)
	{
		printf("\nFile read from client complete, saving and closing file.\n");

		pthread_mutex_lock(&file_info_list_lock);

		// as filename is char pointer, we allocate memory first
		file_info_list[file_info_count].filename = (char *)malloc(strlen(file -> filename + 1));
		bzero(file_info_list[file_info_count].filename, strlen((file -> filename + 1)));
		strcpy(file_info_list[file_info_count].filename, file -> filename);

		file_info_list[file_info_count].filesize = file -> filesize;

		for (int i = 0; i < SHA_DIGEST_LENGTH; i++)
		{
			file_info_list[file_info_count].hash[i] = file -> hash[i];
		}
		free(file);

		int file_info_index = file_info_count;
		file_info_count++;
		pthread_mutex_unlock(&file_info_list_lock);

		if (is_client)
		{
			// get lock on the server peer list, so that while we process no one else updates the list
			pthread_mutex_lock(&server_peer_info_lock);
			for (int i = 0; i <= connected_server_count; i++)
			{
				// take lock for server peer's writer queue
				pthread_mutex_lock(&server_peer_info[i].server_peer_writer_queue_lock);
				// add the file info to the queue
				enqueue(&server_peer_info[i].front, &server_peer_info[i].rear, file_info_list[file_info_index]);
				// signal the writer thread to activate and process the elements we added to queue
				pthread_cond_signal(&server_peer_info[i].server_peer_writer_queue_cv);
				// release the writer queue lock
				pthread_mutex_unlock(&server_peer_info[i].server_peer_writer_queue_lock);
			}
		}

		pthread_mutex_unlock(&server_peer_info_lock);
	}
	else
	{
		printf("Oops!! Something went wrong!! No file from client to upload\n");
	}
}

//================================================================================================================
// for downloading file using search query
//================================================================================================================
void process_query_and_send_file_list(int client_socket)
{
	uint32_t filename_length;
	int read_count = read(client_socket, &filename_length, sizeof(filename_length));
	if(read_count < 0 || !flag)
	{
		printf("Error reading from socket in process_query_and_send_file_list 1.\n");
		return;
	}

	char *query = (char *) malloc(filename_length + 1);
	bzero(query, filename_length + 1);
	read_count = read(client_socket, query, filename_length);
	if(read_count < 0 || !flag)
	{
		printf("Error reading from socket in process_query_and_send_file_list 2.\n");
		return;
	}

	FileInfo filtered_files[MAX];
	uint32_t filtered_file_count = 0;

	pthread_mutex_lock(&file_info_list_lock);
	for (int i = 0; i < file_info_count; i++)
	{
		FileInfo file_info = file_info_list[i];
		if (strstr(file_info.filename, query) != NULL)
		{
			filtered_files[filtered_file_count++] = file_info;
		}
	}
	pthread_mutex_unlock(&file_info_list_lock);

	uint32_t filtered_file_count_nw = htonl(filtered_file_count);
	write(client_socket, &filtered_file_count_nw, sizeof(filtered_file_count));

	for (int i = 0; i < filtered_file_count; i++)
	{
		FileInfo file_info = filtered_files[i];
		BlobInfo blob_info = create_file_info_blob(file_info);

		uint32_t blob_length_nw = htonl(blob_info.bloblength);
		write(client_socket, &blob_length_nw, sizeof(blob_info.bloblength));
		write(client_socket, blob_info.blob, blob_info.bloblength);
	}
}

//================================================================================================================

void *read_from_client(void *socket_info_)
{
	SocketInfo * socket_info = (SocketInfo *) socket_info_;
	int client_socket = socket_info -> socket;
	uint32_t filename_length;


	while (flag)
	{
		// read list, upload, download

		uint8_t message_type;
		if (socket_info -> is_client)
		{
			printf("Waiting for message id from client %d\n", client_socket);
		} else {
			printf("Waiting for message id from server %d\n", client_socket);
		}
		int read_count = read(client_socket, &message_type, sizeof(message_type));
		if(read_count <= 0 || !flag)
		{
			printf("Error reading from socket %d\n", client_socket);
			break;
		}
		if (socket_info -> is_client)
		{
			printf("Got message %d request from client\n", message_type);
		} else {
			printf("Got message %d from another server\n", message_type);
		}


		switch (message_type)
		{

			case 3: // upload file by client
				get_file_uploaded_by_client(client_socket, socket_info -> is_client);
				break;

			case 4:
				send_file_list_to_client(client_socket);
				break;

			case 5: // search by name
				process_query_and_send_file_list(client_socket);
				break;

			case 6:
				read_count = read(client_socket, &filename_length, sizeof(filename_length));
				if(read_count < 0 || !flag)
				{
					printf("Error reading from socket read_from_client 1.\n");
					pthread_exit(&fail);
				}
				filename_length = ntohl(filename_length);

				char *filename_from_client = (char*)malloc(filename_length + 1);
				memset(filename_from_client, 0, filename_length + 1);
				read_count = read(client_socket, filename_from_client, filename_length);
				if(read_count < 0 || !flag)
				{
					printf("Error reading from socket read_from_client 2.\n");
					pthread_exit(&fail);
				}

				int file_info_index = get_file_info_from_filename(filename_from_client);
				if (file_info_index == -1)
				{
					printf("No such file %s found\n", filename_from_client);
					break;
				}
				send_file_to_client(client_socket, file_info_list[file_info_index]);
				break;


			case 7: // disconnect
				close(client_socket);
				pthread_exit(&fail);

			default:
				printf("Invalid message %d from other peer %d\n", message_type, client_socket);
				pthread_exit(&fail);
		}

	}
	if (socket_info -> is_client) {
		printf("Exiting reader thread for client with socket %d", client_socket);
	} else {
		printf("Exiting reader thread for server socket %d", client_socket);
	}
	pthread_exit(&fail);
}

//================================================================================================================
// Accepting connection
//================================================================================================================

int socket_setup(void *port)
{
	server_acceptor_socket = socket(AF_INET, SOCK_STREAM, 0);
	if(server_acceptor_socket == -1)
	{
		printf("Error creating socket.\n");
		exit(-1);
	}
	struct sockaddr_in servaddr;
	servaddr.sin_family = AF_INET;
	servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
	servaddr.sin_port = htons(*(int *)port);

	int yes=1;
	// tell kernel you are okay to re-use the port that is in use
	if (setsockopt(server_acceptor_socket, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes)) == -1) {
		perror("setsockopt error");
		exit(-1);
	}

	//Bind socket to a port to accept connections from client
	if(bind(server_acceptor_socket, (struct sockaddr *)&servaddr, sizeof(servaddr)) < 0)
	{
		printf("Bind failed.\n");
		exit(-1);
	}

	//printf("Socket binding complete.\n");
	listen(server_acceptor_socket, 3);
	return server_acceptor_socket;
}

//*****************************************************************************************************************************
// NEW FEATURE IMPLEMENTED by 1731 - Khushbu Lohiya - Individual Report Contribution
//*****************************************************************************************************************************

int inform_head_server()
{
	int server_socket = connect_to_server(head_server_address, head_server_port);
	if (server_socket == -1)
	{
		printf("Error connecting to server. \n Exiting..!!\n");
		exit(-1);
	}
	printf("Connected to head server on %s:%d\n", head_server_address, head_server_port);
	write(server_socket, &I_AM_SERVER_HELLO, sizeof(I_AM_SERVER_HELLO));
	printf("Sent Server Hello Message %d to head server\n", I_AM_SERVER_HELLO);
	send_server_info(server_socket, *my_info);
	return 0;
}

//================================================================================================================================
void *acceptor_thread(void* port)
{
	int server_acceptor_socket = socket_setup(port);

	// once socket is setup to accept connections from client, 
	// we inform the head server
	int res = inform_head_server();

	struct sockaddr_in cliaddr;

	printf("Waiting for connections from client and server.\n");
	int size = sizeof(struct sockaddr_in);

	pthread_t reader_threads_t[MAX_READER_THREADS];
	pthread_t writer_threads_t[MAX_WRITER_THREADS];
	int reader_thread_count = 0;
	int writer_thread_count = 0;

	while(flag)
	{
		//Accept the incoming connections from client or server
		int new_socket = accept(server_acceptor_socket, (struct sockaddr *)&cliaddr, (socklen_t *)&size);
		if(new_socket < 0)
		{
			printf("Connection failed in accept.\n");
			break;
		}
		accepted_sockets[accepted_sockets_count++] = new_socket;

		// get the sender type, I_AM_SERVER = 0x01 or I_AM_CLIENT = 0x02
		uint8_t sender_type;
		int read_count = read(new_socket, &sender_type, sizeof(sender_type));
		if(read_count < 0 || !flag)
		{
			printf("Error reading from accepted socket.\n");
			break;
		}

		// if its server, we store the information of the server in list
		// as we might need to send updates to the server
		if (sender_type == I_AM_SERVER_HELLO)
		{
			printf("Accepted connection from another server with socket %d\n", new_socket);

			// get the length of the sender's name
			uint8_t sender_id_length;
			int read_count = read(new_socket, &sender_id_length, sizeof(sender_id_length));
			if (read_count < 0)
			{
				printf("Error reading from accepted socket\n");
				break;
			}
			// allocate memory to store the sender's name
			char *sender_name = (char *)malloc(sender_id_length + 1);
			memset(sender_name, 0, sender_id_length + 1);

			// read the sender name from sender
			read_count = read(new_socket, sender_name, sender_id_length);
			if (read_count < 0)
			{
				printf("Error reading from accepted socket\n");
				break;
			}

			printf("Connection accepted from server %s\n", sender_name);

			// we will need lock for this, as multiple threads could be reading at the same time and
			// will access this data structure server_peer_info and connected_server_count
			pthread_mutex_lock(&server_peer_info_lock);
			connected_server_count++;
			server_peer_info[connected_server_count].connected = True;
			server_peer_info[connected_server_count].server_name = sender_name;
			server_peer_info[connected_server_count].socket = new_socket;
			server_peer_info[connected_server_count].front = NULL;
			server_peer_info[connected_server_count].rear = NULL;
			pthread_mutex_init(&server_peer_info[connected_server_count].server_peer_writer_queue_lock, NULL);
			pthread_cond_init(&server_peer_info[connected_server_count].server_peer_writer_queue_cv, NULL);
			pthread_mutex_unlock(&server_peer_info_lock);

			//Create thread to read file info from client and store the number of files_count
			//printf("Creating read_from_server thread in acceptor for %d\n", new_socket);
			SocketInfo * socket_info = (SocketInfo*)malloc(sizeof(SocketInfo));
			socket_info -> socket = new_socket;
			socket_info -> is_client = False;

			int val = pthread_create(&reader_threads_t[reader_thread_count], NULL, (void*)&read_from_client, (void*)socket_info);
			if(val < 0)
			{
				printf("Error creating acceptor thread.\n");
				break;
			}
			printf("Reader thread created for accepted server %s\n", server_name);
			reader_thread_count++;

			//Create thread to send the file to the server and for dequeue operation
			//printf("Creating writer_thread thread in acceptor for %d\n", new_socket);
			val = pthread_create(&writer_threads_t[writer_thread_count], NULL, (void*)&write_to_server, (void*)&(server_peer_info[connected_server_count]));
			if(val < 0)
			{
				printf("Error creating acceptor thread.\n");
				break;
			}
			printf("Writer thread created for accepted server %s\n", server_name);
			writer_thread_count++;
		}
		else
		{
			printf("Accepted connection from another client on socket %d\n", new_socket);

			// Display client address and port so that server can keep track of what address client is connected to
			// create thread to perform the actions as requested by the client by taking into account the message id
			// printf("Creating read_from_client thread in acceptor for %d\n", new_socket);
			SocketInfo * socket_info = (SocketInfo*)malloc(sizeof(SocketInfo));
			socket_info -> socket = new_socket;
			socket_info -> is_client = True;
			int val = pthread_create(&reader_threads_t[reader_thread_count], NULL, (void*)&read_from_client, (void*)socket_info);
			if(val < 0)
			{
				printf("Error creating client reader thread.\n");
				break;
			}
			printf("Reader thread created for client\n");
			reader_thread_count++;
		}
	}
	close(server_acceptor_socket);

	// for loop for reader threads pthread_wait
	for (int i = 0; i < reader_thread_count; i++)
	{
		pthread_join(reader_threads_t[i], NULL);
	}
	// for loop for writer threads pthread_wait
	for (int i = 0; i < writer_thread_count; i++)
	{
		pthread_join(writer_threads_t[i], NULL);
	}
	printf("Program Exiting!! Acceptor thread exiting\n");
	pthread_exit(&fail);
}

//================================================================================================================
// For Mesh Network
//================================================================================================================

void initiate_connections_to_other_servers()
{
	// create array for reader thread ids
	// create array for writer thread ids
	// create array for all sockets
	pthread_t reader_thread_t[MAX_READER_THREADS];
	pthread_t writer_thread_t[MAX_WRITER_THREADS];
	int reader_thread_count = 0;
	int writer_thread_count = 0;
	int connected_sockets[MAX_SERVERS];
	int connected_socket_count = -1;

	bool first_server = True;
	for (int i = 0; i < total_server_count; i++)
	{
		if (strcmp(servers[i].server_name, server_name) == 0)
		{
			// no need to connect itself, so we skip
			printf("%s is the server itself, so skipping connecting to it.\n", servers[i].server_name);
			continue;
		}

		//create socket for interaction with other servers joining the mesh network
		int server_socket = socket(AF_INET, SOCK_STREAM, 0);
		if(server_socket == -1)
		{
			printf("Error creating socket.\n");
			exit(-1);
		}
		struct sockaddr_in servaddr;
		servaddr.sin_family = AF_INET;
		servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
		servaddr.sin_port = htons(servers[i].server_port);

		//connect to other servers
		if(connect(server_socket, (struct sockaddr *)&servaddr, sizeof(servaddr)) < 0)
		{
			printf("Connection failed to server. May be server %s is not up. Lazy Server!!\n", servers[i].server_name);
			continue;
		}
		printf("Connected to server %s on socket %d\n", servers[i].server_name, server_socket);
		connected_to_sockets[connected_to_sockets_count++] = server_socket;

		connected_sockets[++connected_socket_count] = server_socket;

		//After connection acquire the lock and store the newly connected server information
		pthread_mutex_lock(&server_peer_info_lock);
		connected_server_count++;
		server_peer_info[connected_server_count].connected = True;
		server_peer_info[connected_server_count].server_name = servers[i].server_name;
		server_peer_info[connected_server_count].socket = server_socket;
		server_peer_info[connected_server_count].front = NULL;
		server_peer_info[connected_server_count].rear = NULL;
		server_peer_info[connected_server_count].get_files = first_server;

		pthread_mutex_init(&server_peer_info[connected_server_count].server_peer_writer_queue_lock, NULL);
		pthread_cond_init(&server_peer_info[connected_server_count].server_peer_writer_queue_cv, NULL);
		//release the lock after storing the server information
		pthread_mutex_unlock(&server_peer_info_lock);

		// tell other server that you are connecting to it and you are server
		int write_count = write(server_socket, &I_AM_SERVER_HELLO, sizeof(I_AM_SERVER_HELLO));
		if(write_count < 0)
		{
			printf("Error writing on socket.\n");
		}
		// send the server name length
		uint8_t sender_id_length = strlen(server_name);
		write(server_socket, &sender_id_length, sizeof(sender_id_length));

		// write the sender name
		write(server_socket, server_name, sender_id_length);

		// create reader server thread
		//printf("Creating read_from_server in connector thread for %d\n", server_socket);
		SocketInfo * socket_info = (SocketInfo*)malloc(sizeof(SocketInfo));
		socket_info -> socket = server_socket;
		socket_info -> is_client = False;
		if (first_server) {
			get_files_update(socket_info);
		}
		first_server = False;
		int val = pthread_create(&reader_thread_t[reader_thread_count], NULL, (void *)read_from_client, (void *)socket_info);
		if(val < 0)
		{
			printf("Error creating read_from_server thread.\n");
			exit(-1);
		}
		reader_thread_count++;

		// create writer thread
		//printf("Creating writer_thread thread in connector for %d\n", server_socket);
		val = pthread_create(&writer_thread_t[writer_thread_count], NULL, (void *)write_to_server, (void *) &(server_peer_info[connected_server_count]));
		if(val < 0)
		{
			printf("Error creating writer_thread.\n");
			exit(-1);
		}
		writer_thread_count++;
	}
	// for loop for reader threads pthread_wait
	for (int i = 0; i < reader_thread_count; i++)
	{
		pthread_join(reader_thread_t[i], NULL);
	}
	// for loop for writer threads pthread_wait
	for (int i = 0; i < writer_thread_count; i++)
	{
		pthread_join(writer_thread_t[i], NULL);
	}
}

//================================================================================================================

//*****************************************************************************************************************************
// Linked List 
//*****************************************************************************************************************************

struct ListNode {
	ServerInfo *server_info;
	struct ListNode *next;
};

struct ListNode *head_ll = NULL;
struct ListNode *tail_ll = NULL;
struct ListNode *current = NULL;

//*****************************************************************************************************************************
// Head Server to exchange server info with server and client
//
//*****************************************************************************************************************************
void main_server_acceptor()
{
	int server_acceptor_socket = socket_setup(&head_server_port);
	struct sockaddr_in cliaddr;

	printf("Head Server waiting for connections from client and server.\n");
	int size = sizeof(struct sockaddr_in);

	while(flag)
	{
		//Accept the incoming connections from client or server
		int new_socket = accept(server_acceptor_socket, (struct sockaddr *)&cliaddr, (socklen_t *)&size);
		if(new_socket < 0)
		{
			printf("Connection failed in accept.\n");
			break;
		}

		uint8_t message_id;
		read(new_socket, &message_id, sizeof(message_id));

		if (message_id == M_ID_GET_PEER_SERVER_INFO_REQ)
		{
			printf("Accepted connection from client\n");

			printf("Received Message id %d from client\n", message_id);
			write(new_socket, &M_ID_GET_PEER_SERVER_INFO_RES, sizeof(M_ID_GET_PEER_SERVER_INFO_RES));
			printf("Returning response to client's request with message id %d\n", M_ID_GET_PEER_SERVER_INFO_RES);
			//send_server_info(new_socket, servers[server_index]);
			send_server_info(new_socket, *(current -> server_info));
			current = current -> next;

		}
		
		else if (message_id == I_AM_SERVER_HELLO)
		{
			printf("Accepted connection from server\n");

			ServerInfo *peer_server_info = read_server_info(new_socket);
			struct ListNode *node = (struct ListNode*)malloc(sizeof(struct ListNode));
			node -> server_info = peer_server_info;
			if (tail_ll == NULL) {
				head_ll = tail_ll = node;
				tail_ll -> next = node;
				current = head_ll;
			} else {
				node -> next = head_ll;
				tail_ll -> next = node;
				tail_ll = node;
			}
			printf("Appended server info to linked list -->");
			printf("server name=%s server ip=%s port=%d\n",
				   peer_server_info->server_name, peer_server_info->server_ip, peer_server_info->server_port);
		}
		else
		{
			// unsupported message
			//print error, return error message id to client
			printf("Invalid request message id %d\n", message_id);
			close(new_socket);
		}
	}
}


int main(int argc, char* argv[])
{
	//To catch signal(graceful shutdown when control C is pressed)
	signal(SIGINT, signal_handler);

	if (argc == 2)
	{
		// if ./server s1 or something like this
		// it is server
		initialize_server_config();
		server_name = argv[1];

		my_info = get_my_config(server_name);
		if (my_info == NULL)
		{
			printf("No configuration found for server name %s\n", server_name);
			exit(-1);
		}

		//Initialize and declaring the associated mutex lock
		pthread_mutex_init(&server_peer_info_lock, NULL);
		pthread_mutex_init(&file_info_list_lock, NULL);

		pthread_t acceptor_thread_t;

		//create acceptor thread to accept connection client as well as server to form mesh network
		int val = pthread_create(&acceptor_thread_t, NULL, (void*)&acceptor_thread, (void*)&my_info -> server_port);
		if(val < 0)
		{
			printf("Error creating acceptor thread.\n");
			exit(-1);
		}

		initiate_connections_to_other_servers();
		pthread_join(acceptor_thread_t, NULL);
	}
	else if(argc == 1) { // New Feature else block for head server
		// if no parameters then it is head server
		main_server_acceptor();
	}
	else
	{
		printf("\nUsage: %s <server_name>\n", argv[0]);
		printf("OR\nUsage: %s\n", argv[0]);
	}
	
}
//================================================================================================================































