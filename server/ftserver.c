/******************************************************************************************
** Programming Assignment #2												     ftserver.c
** Author: Kara Franco
** CS.372-400 Intro to Computer Networks
** Due Date: March 6, 2016
** Description: A server program that connects with one client at a time to list and transfer 
** files. The server runs continuously between each client connection. Each client session is 
** run on the control connection and the file transfer/listing is run on a data connection. 
** The server is stopped upon receiving an interrupt signal. 
** Go to sources: (detailed citing within program):
** http://beej.us/guide/bgnet/output/html/multipage/index.html
** https://www.ietf.org/rfc/rfc959.txt
******************************************************************************************/

// assert()
#include <assert.h>
// dirent struct, opendir, readdir, closedir
#include <dirent.h>
// to use all symbols from <netinet/in.h>
#include <netdb.h>
// sigaction, 
#include <signal.h>
// printf, sscanf, fread(), fclose()
#include <stdio.h>
// malloc(), realloc(), atoi()
#include <stdlib.h>
// strlen, strcopy, strcmp, memset
#include <string.h>
// socket(), socklen_t, send()
#include <sys/socket.h>
// stat struct
#include <sys/stat.h>
// close()
#include <unistd.h>
// sockaddr_in, sin_port, htons, etc.
#include <netinet/in.h>
//inet_ntoa()
#include <arpa/inet.h>


/******************************* Global Variables ****************************************/
// number of bytes in packet payload
// number suggested in this article: https://en.wikipedia.org/wiki/Trivial_File_Transfer_Protocol
#define PAYLOAD_LENGTH		512 
// number of bytes for tag field (used to identify user commands)
#define TAG_LENGTH            8
// number of bytes for packet length field
#define PACKET_SIZE		      2

/***************************** Function Declarations *************************************/

int isNumber(char *str, int *n);
void stopServer(int sig);
void receiveData(int socket, void *buffer, int size);
void receivePacket(int socket, char *tag, char *data);
char **listFiles(char *dirName, int *numberFiles);
int controlConnection(int controlSocket, char *userCommand, int *dataPort, char* filename);
int dataConnection(int controlSocket, int dataSocket, char *userCommand, char *filename);
void sendData(int socket, void *buffer, int numberBytes);
void handleRequest(int socket, char *tag, char *data);
void startServer(int port);

/********************************** Main Function ****************************************/

int main(int argc, char **argv){
	// port number that listens for client connections
	int port; 
	// check for server user input errors 
	// server expects two command-line arguments, "ftserver" and the desired port number
	if (argc != 2) {
		fprintf(stderr, "Error: Use ftserver <server-port>\n");
		exit(1);
	}
	// port number must be a number
	if (!isNumber(argv[1], &port)) {
		fprintf(stderr, "ftserver: Server port must be a number!\n");
		exit(1);
	}
	// start the server until an interrupt signal is received
	startServer(port);

	exit(0);
}

/****************************** Function Definitions ************************************/

/******************************************************************************
** isNumber()
** Description: A function that determines if the server user input string contains 
** numbers or letters. Used in Main function. 
** Parameters: server user input string, port number (holder)
** Output: true if number in string matches an int
******************************************************************************/
int isNumber(char *str, int *n){
	char c;
	// convert the string to an integer and a character
	// source for storing result: http://www.tutorialspoint.com/c_standard_library/c_function_sscanf.htm
	int stringMatches = sscanf(str, "%d %c", n, &c);
	// a string number will result in only one match
	return stringMatches == 1;
}

/******************************************************************************
** stopServer()
** Description: A function that terminates the program and gives feedback before
** terminating. Invoked in the startServer function. 
** Parameters: signal number
** Output: the program ends with a goodbye message
******************************************************************************/
void stopServer(int sig){
	int status;  
	// signal action for handling interrupt
	// source: http://pubs.opengroup.org/onlinepubs/009695399/functions/sigaction.html
	struct sigaction interrupt;   
	// say goodbye to the server user
	printf("\nftserver: Server closed, have a nice day!\n");

	// set interrupt handle to specify default action
	// source: ftp://ftp.gnu.org/old-gnu/Manuals/glibc-2.2.3/html_chapter/libc_24.html
	interrupt.sa_handler = SIG_DFL;
	status = sigaction(SIGINT, &interrupt, 0);
	if (status == -1) {
		perror("sigaction");
		exit(1);
	}
	// use raise() to generate SIGINT to stop current process  
	// http://pubs.opengroup.org/onlinepubs/009695399/functions/raise.html
	status = raise(SIGINT);
	if (status == -1) {
		perror("raise");
		exit(1);
	}
}

/******************************************************************************
** listFiles()
** Description: A function that lists all the files in the directory of the 
** server. Invoked in dataConnection(), Program lists the file names to the 
** client screen. 
** Parameters: direectory name, and the number of files in the directory
** Output: list of filenames
******************************************************************************/
char ** listFiles(char *dirName, int *numberFiles){
	// set up return variable
	char **fileList;      
	DIR *dir; 
	// set up directory variables
	// source directory format: http://pubs.opengroup.org/onlinepubs/007908775/xsh/dirent.h.html
	struct dirent *entry; 
	struct stat info;     
	// open the current directory
	dir = opendir(dirName);
	if (dir == NULL) {
		fprintf(stderr, "ftserver: Error, cannot open %s\n", dirName);
		exit(1);
	}
	// create the list of filenames in the current directory
	*numberFiles = 0;
	fileList = NULL;
	while ((entry = readdir(dir)) != NULL) {
		// if there are subdirectories, ignore them
		// source: http://www.gnu.org/software/libc/manual/html_node/Testing-File-Type.html
	    // source: http://pubs.opengroup.org/onlinepubs/009695399/basedefs/sys/stat.h.html
		stat(entry->d_name, &info);
		if (S_ISDIR(info.st_mode)) {
			continue;
		}

		{
			// -- attach the current filename onto the list --
			// allocate memory for new file name
			if (fileList == NULL) {
				fileList = malloc(sizeof(char *));
			}
			else {
				fileList = realloc(fileList, (*numberFiles + 1) * sizeof(char *));
			}
			// check for malloc() error
			assert(fileList != NULL); 
			fileList[*numberFiles] = malloc((strlen(entry->d_name) + 1) * sizeof(char));
			assert(fileList[*numberFiles] != NULL); 
			// copy the filename into the fileList
			strcpy(fileList[*numberFiles], entry->d_name);
			// increase the number of files listed
			(*numberFiles)++;
		}
	}
	// close the current directory
	closedir(dir);
	return fileList;
}

/******************************************************************************
** receiveData()
** Description: A function that uses the recv() function to collect all the data
** that is being sent. This function is used in the receivePacket function below.
** Parameters: current socket endpoint to receive data, number of bytes to receive,
** buffer to hold the number of bytes, changed.
** Output: none
******************************************************************************/
void receiveData(int socket, void *buffer, int numberBytes){
	// set up variables for recv(); ret return val
	int ret;  
	// total number of bytes received
	int receivedBytes;     
	// find the number of bytes received
	receivedBytes = 0;
	while (receivedBytes < numberBytes) {
		ret = recv(socket, buffer + receivedBytes, numberBytes - receivedBytes, 0);
		// if error in recv, exit
		if (ret == -1) {
			perror("recv");
			exit(1);
		}
		// add the number of bytes received, this will total the buffer needed
		else {
			receivedBytes += ret;
		}
	}
}

/******************************************************************************
** receivePacket()
** Description: A function that receives a packet from the socket after the
** connection is made. Used in the control connection.
** Parameters: current socket endpoint to receive data, the 8 byte tag (has info 
** about what action requested, data to transfer; data and tag are changed
** Output: none
** Source: http://beej.us/guide/bgnet/output/html/singlepage/bgnet.html#sonofdataencap
******************************************************************************/
void receivePacket(int socket, char *tag, char *data){
	// set up variables for receiveing packet
	// number of bytes in packet
	unsigned short packetLength; 
	// number of bytes in the data encapsulated
	unsigned short dataLength;
	// tempTag buffer made
	char tempTag[TAG_LENGTH + 1];  
	// tempPayload buffer made
	char tempPayload[PAYLOAD_LENGTH + 1]; 
	// call receiveData() to get packet length
	receiveData(socket, &packetLength, sizeof(packetLength));
	// source for byte order conversion: https://www.gnu.org/software/libc/manual/html_node/Byte-Order.html 
	packetLength = ntohs(packetLength);
	// call receiveData() to get the tag length
	receiveData(socket, tempTag, TAG_LENGTH);
	tempTag[TAG_LENGTH] = '\0';
	if (tag != NULL) { strcpy(tag, tempTag); }
	// call receiveData() to get the payload (data)
	dataLength = packetLength - TAG_LENGTH - sizeof(packetLength);
	receiveData(socket, tempPayload, dataLength);
	tempPayload[dataLength] = '\0';
	if (data != NULL) { strcpy(data, tempPayload); }
}

/******************************************************************************
** controlConnection()
** Description: A function that runs the connection to the client. The program 
** prints to the client screen what actions are taking place, and changes the 
** userCommand, dataPort and filename. Used in startServer.
** Parameters: current socket endpoint to receive data, user command (from tag), 
** data port number, and the filename
** Output: 0 success, -1 error
******************************************************************************/
int controlConnection(int controlSocket, char *userCommand, int *dataPort, char* filename){
	// set up variables, buffer for input and output
	char dataIn[PAYLOAD_LENGTH + 1];  
	char tagIn[TAG_LENGTH + 1];          
	char dataOut[PAYLOAD_LENGTH + 1]; 
	char tagOut[TAG_LENGTH + 1];        
	// get the command(tag) and data
	// call receivePacket() to get the data port
	printf("  Receiving data port...\n");
	receivePacket(controlSocket, tagIn, dataIn);
	if (strcmp(tagIn, "DPORT") == 0) { *dataPort = atoi(dataIn); }
	// call receivePacket to get client user command and filename
	printf("  Receiving user command ...\n");
	receivePacket(controlSocket, tagIn, dataIn);
	strcpy(userCommand, tagIn);
	strcpy(filename, dataIn);
	// check if user entered the command (either -l or -g) correctly 
	if (strcmp(tagIn, "LIST") != 0 && strcmp(tagIn, "GET") != 0) {
		printf("  Sending command error ...\n");
		strcpy(tagOut, "ERROR");
		strcpy(dataOut, "Command must be either -l or -g");
		handleRequest(controlSocket, tagOut, dataOut);
		return -1;
	}
	// after the above collects the: port, client user command and checks for errors,
	// send the socket and client user command to handle the request (which sends data/file)
	else {
		printf("  Sending okay for data connection...\n");
		strcpy(tagOut, "OKAY");
		handleRequest(controlSocket, tagOut, "");
		return 0;
	}
}

/******************************************************************************
** dataConnection()
** Description: A function that runs the client file transfer connection. The program 
** prints to the client screen what actions are taking place. Used in startServer().
** Parameters: server endpoints (2) for data connection and for control connection, 
** the user command (from tag), and the filename to be transferred (if requested)
** Output: 0 success, -1 error
******************************************************************************/
int dataConnection(int controlSocket, int dataSocket, char *userCommand, char *filename){
	int ret = 0; 
	// list of filenames and the number of files in the current dir
	char **fileList; 
	int numberFiles;   
	// call listFiles() to get the files in the current directory
	fileList = listFiles(".", &numberFiles);
	// if client user command to list the filenames, then call handleRequest() to send names
	if (strcmp(userCommand, "LIST") == 0) {
		// transfer each name in it's own packet
		printf("  Sending file listing ...\n");
		for (int i = 0; i < numberFiles; i++) {
			handleRequest(dataSocket, "FNAME", fileList[i]);
		}
	}
	// if client user command to get a file, 
	// then call handleRequest() to either send file or error/already exists
	else if (strcmp(userCommand, "GET") == 0) {
		do {
			char buffer[PAYLOAD_LENGTH + 1]; 
			// make variables for handling files
			// number of bytes in file (use fread() to get)
			int bytesRead;  
			// used to set if file exists or not
			int fileExists; 
			FILE *infile;   
			// for all files in directory, compare the filename with the names in dir list
			fileExists = 0;
			for (int i = 0; i < numberFiles && !fileExists; i++) {
				if (strcmp(filename, fileList[i]) == 0) {
					fileExists = 1;
				}
			}
			// if the filename does not exsist, send error
			if (!fileExists) {
				printf("  Sending file error ...\n");
				handleRequest(controlSocket, "ERROR", "Error: File not found");
				ret = -1;
				break;
			}

			// open file, if file will not open, send error
			infile = fopen(filename, "r");
			if (infile == NULL) {
				printf("  Sending cannot open error ...\n");
				handleRequest(controlSocket, "ERROR", "Error: cannot open file");
				ret = -1;
				break;
			}
			// call handleRequest() to transfer name of file
			handleRequest(dataSocket, "FILE", filename);

			// call handleRequest() to transfer the actual file
			// source: http://stackoverflow.com/questions/8589425/how-does-fread-really-work
			printf("  Sending file ...\n");
			do {
				bytesRead = fread(buffer, sizeof(char), PAYLOAD_LENGTH, infile);
				buffer[bytesRead] = '\0';
				handleRequest(dataSocket, "FILE", buffer);
			} while (bytesRead > 0);
			if (ferror(infile)) {
				perror("fread");
				ret = -1;
			}
			fclose(infile);

		} while (0);
	}
	// if we get here, there is an error in the client user command tag
	else {
		fprintf(stderr, "ftserver: User command must be \"LIST\" or "
			"\"GET\"; received \"%s\"\n", userCommand);
		ret = -1;
	}
	// final FT tag must be labeled DONE, so that the client knows transfer is complete
	handleRequest(dataSocket, "DONE", "");
	// user (client) is sent close request
	printf("  Sending okay for closing connection...\n");
	handleRequest(controlSocket, "CLOSE", "");
	// free memory from fileList
	for (int i = 0; i < numberFiles; i++) {
		free(fileList[i]);
	}
	free(fileList);
	return ret;
}

/******************************************************************************
** sendData()
** Description: A function that uses send() to transfer all the bytes of data to 
** the client. Used in handleRequest.
** Parameters: current socket endpoint to send from, the data buffer and the number
** of bytes to be sent.
** Output: none
******************************************************************************/
void sendData(int socket, void *buffer, int numberBytes){
	// set up variables for send(); ret return val
	int ret;    
	// number of bytes to be sent
	int sentBytes;    
	// send the number of bytes
	// source: http://pubs.opengroup.org/onlinepubs/009695399/functions/send.html
	sentBytes = 0;
	while (sentBytes < numberBytes) {
		ret = send(socket, buffer + sentBytes, numberBytes - sentBytes, 0);
		// if send() returns an error, exit
		if (ret == -1) {
			perror("send");
			exit(1);
		}
		// add the data sent (from ret) to sent data
		else {
			sentBytes += ret;
		}
	}
}

/******************************************************************************
** handleRequest()
** Description: A function that sends packet(s) from the server to the client. 
** Used in controlConnection() and dataConnection().
** Parameters: current socket endpoint to send from, the data buffer and the tag
** Output: none
******************************************************************************/
void handleRequest(int socket, char *tag, char *data){
	// set variables for handling packets
	// number of bytes in packet
	unsigned short packetLength;
	// tag buffer (set at 8 bytes)
	char tagBuffer[TAG_LENGTH];            
	// call sendData() to send the packet length
	packetLength = htons(sizeof(packetLength) + TAG_LENGTH + strlen(data));
	sendData(socket, &packetLength, sizeof(packetLength));
	// call sendData() to send the tag
	memset(tagBuffer, '\0', TAG_LENGTH);   
	strcpy(tagBuffer, tag);
	sendData(socket, tagBuffer, TAG_LENGTH);
	// finally, call sendData() to send the list/files (will call many times)
	sendData(socket, data, strlen(data));
}

/******************************************************************************
** startServer()
** Description: A function that listens on inputed port, and interacts with 
** one client user, and remains open after the user connection terminates for 
** more client users. Used in main function. 
** Parameters: port that server user enters
** Output: none
******************************************************************************/
void startServer(int port){
	// endpoint socket that receives requests
	int serverSocket;                 
	int status;  
	// set up signal for handling ctrl c exit
	struct sigaction interrupt; 
	// get server address
	struct sockaddr_in serverAddress; 
	// specify what type of socket addressing used (IPv4)
	serverAddress.sin_family = AF_INET; 
	// convert address host to network short
	serverAddress.sin_port = htons(port);   
	// bind socket to INNADD_ANY (all available interfaces, not just localhost)
	// source: http://stackoverflow.com/questions/16508685/understanding-inaddr-any-for-socket-programming-c 
	serverAddress.sin_addr.s_addr = INADDR_ANY; 
	// set the server side socket
	serverSocket = socket(AF_INET, SOCK_STREAM, 0);
	if (serverSocket == -1) {
		perror("socket");
		exit(1);
	}
	// bind the new server socket with the port
	status = bind(serverSocket, (struct sockaddr*) &serverAddress, sizeof(serverAddress));
	if (status == -1) {
		perror("bind");
		exit(1);
	}
	// listen for client user connections, up to 10
	status = listen(serverSocket, 10);
	if (status == -1) {
		perror("listen");
		exit(1);
	}
	// ready stopServer() to catch any interrupt signals
	interrupt.sa_handler = &stopServer;
	interrupt.sa_flags = 0;
	sigemptyset(&interrupt.sa_mask);
	status = sigaction(SIGINT, &interrupt, 0);
	if (status == -1) {
		perror("sigaction");
		exit(1);
	}
	// start running on port, waiting for client user connections (data or control)
	printf("ftserver: Server open on port %d\n", port);
	while (1) {
		// variable for printing client IP (in decimal form)
		char *clientIPv4; 
		// variables for command tags and filename buffers
		char userCommand[TAG_LENGTH + 1];      
		char filename[PAYLOAD_LENGTH + 1]; 
		// server side socket endpoints
		int controlSocket, dataSocket;   
		// client data port number
		int dataPort;
		// address struct length
		// http://pubs.opengroup.org/onlinepubs/7908799/xns/syssocket.h.html
		socklen_t addrLen;                 
		struct sockaddr_in clientAddress;  
		// start file transfer connection with client
		addrLen = sizeof(struct sockaddr_in);
		controlSocket = accept(serverSocket, (struct sockaddr *) &clientAddress, &addrLen);
		if (controlSocket == -1) {
			perror("accept");
			exit(1);
		}
		// 
		clientIPv4 = inet_ntoa(clientAddress.sin_addr);
		printf("\nftserver: Control connection established with \"%s\"\n", clientIPv4);
		// controlConnection() to send/receive packets with client user requests
		status = controlConnection(controlSocket, userCommand, &dataPort, filename);
		// if packets understood, proceed to the data connection (where fileList or files are sent)
		if (status != -1) {
			int connectionAttempts; 
			// set server side endpoint for data connection
			dataSocket = socket(AF_INET, SOCK_STREAM, 0);
			if (dataSocket == -1) {
				perror("socket");
				exit(1);
			}
			// start data connection with client user
			clientAddress.sin_port = htons(dataPort);
			connectionAttempts = 0;
			do {
				status = connect(dataSocket, (struct sockaddr *) &clientAddress, sizeof(clientAddress));
			// max number of connection attempts is set at 10
			} while (status == -1 && connectionAttempts < 10);
			if (status == -1) {
				perror("connect");
				exit(1);
			}
			printf("ftserver: Data connection established with \"%s\"\n", clientIPv4);

			// call dataConnection to transfer file list or files to client user
			dataConnection(controlSocket, dataSocket, userCommand, filename);
			// wait to receive packet from user that the trasfer or listing is DONE (in tag)
			receivePacket(controlSocket, NULL, NULL);
			// close the data connection, client can reconnect for more interactions
			status = close(dataSocket);
			if (status == -1) {
				perror("close");
				exit(1);
			}
			printf("ftserver: Data connection closed\n");
		}
	}
}