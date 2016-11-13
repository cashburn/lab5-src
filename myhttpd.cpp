const char * usage =
"                                                               \n"
"   Usage: myhttpd [-f|-t|-p] <port>                                       \n"
"                                                               \n"
"Where 1024 < port < 65536.             						\n"
"                                                               \n";

const char * successHeader =
"HTTP/1.0 200 OK\n"
"Server: cashburn\n";

const char * errorHeader =
"HTTP/1.0 404 Not Found\n"
"Content-Type: text/html\n"
"Server: cashburn\n";

const char * errorPage = "<!DOCTYPE html><title>Error</title><p><b>Error</b></p>";

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <netinet/in.h>
#include <netdb.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <time.h>
#include <signal.h>
#include <pthread.h>

#define MAXPATH 1024
#define MAXHEAD 4096

int QueueLength = 5;
pthread_mutex_t mutex;

// Processes request
void processRequest(int socket);
//Thread wrapper
void processRequestThread(void * socket);

void poolSlave(void * socket);
//Content-Type helper function
char * setContentType(char * path);

void sigChldHandler(int sig);

void sigPipeHandler(int sig);

int main( int argc, char ** argv ) {
	int port;
  	//Print usage if not enough arguments
	if ( argc < 2 || argc > 3) {
		fprintf( stderr, "%s", usage );
		exit( -1 );
	}

	if (signal(SIGCHLD, sigChldHandler) == SIG_IGN)
		signal(SIGCHLD, SIG_IGN);

	if (signal(SIGPIPE, sigPipeHandler) == SIG_IGN)
		signal(SIGPIPE, SIG_IGN);

	if (argc == 2) {
  		//Get the port from the arguments
		port = atoi( argv[1] );
	}

	else if (argc == 3) {
		//Get the port from the arguments
		port = atoi(argv[2]);
	}

	if (port == 0) {
		fprintf( stderr, "%s", usage );
		exit( -1 );
	}

  	//Set the IP address and port for this server
	struct sockaddr_in serverIPAddress;
	memset( &serverIPAddress, 0, sizeof(serverIPAddress) );
	serverIPAddress.sin_family = AF_INET;
	serverIPAddress.sin_addr.s_addr = INADDR_ANY;
	serverIPAddress.sin_port = htons((u_short) port);

  	//Allocate a socket
	int masterSocket =  socket(PF_INET, SOCK_STREAM, 0);
	if ( masterSocket < 0) {
		perror("socket");
		exit( -1 );
	}

  	//Set socket options to reuse port. Otherwise we will
  	//have to wait about 2 minutes before reusing the sae port number
	int optval = 1;
	int err = setsockopt(masterSocket, SOL_SOCKET, SO_REUSEADDR,
		(char *) &optval, sizeof( int ) );

  	//Bind the socket to the IP address and port
	int error = bind( masterSocket,
		(struct sockaddr *)&serverIPAddress,
		sizeof(serverIPAddress) );
	if ( error ) {
		perror("bind");
		exit( -1 );
	}

  	//Put socket in listening mode and set the
  	//size of the queue of unprocessed connections
	error = listen( masterSocket, QueueLength);
	if ( error ) {
		perror("listen");
		exit( -1 );
	}

	if (argc == 3 && !strcmp(argv[1], "-p")) {
		pthread_t tid[5];
		pthread_attr_t attr;
		pthread_mutex_init(&mutex, NULL);
		pthread_attr_init(&attr);
		pthread_attr_setscope(&attr, PTHREAD_SCOPE_SYSTEM);
		for (int i = 0; i < 5; i++) {
			pthread_create(&tid[i], &attr,
				(void * (*)(void *)) poolSlave,
				(void *) (intptr_t) masterSocket);
		}
		pthread_join(tid[0], NULL);
		//poolSlave((void *)masterSocket);
		perror("poolSlave");
		exit(-1);
	}

	while ( 1 ) {

    	//Accept incoming connections
		struct sockaddr_in clientIPAddress;
		int alen = sizeof( clientIPAddress );
		int slaveSocket = accept( masterSocket,
			(struct sockaddr *)&clientIPAddress,
			(socklen_t*)&alen);

		if (slaveSocket < 0) {
			perror( "accept" );
			exit( -1 );
		}

		if (argc == 2) {
			//Process request.
			processRequest( slaveSocket );
			//Close socket
			close(slaveSocket);
		}

		else if (!strcmp(argv[1], "-f")) {
			int pid;
			if (!(pid = fork())) {
				//Process request.
				processRequest( slaveSocket );
		    	//Close socket
				close(slaveSocket);
				exit(0);
			}

			close(slaveSocket);
		}

		else if (!strcmp(argv[1], "-t")) {
			int pid;
			pthread_t t;
			pthread_attr_t attr;
			pthread_attr_init(&attr);
			pthread_attr_setscope(&attr, PTHREAD_SCOPE_SYSTEM);
			pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
			pthread_create(&t, &attr,
				(void * (*)(void *)) processRequestThread, (void *) (intptr_t) slaveSocket);
		}
	}
}

void processRequestThread(void * socket) {
	processRequest((intptr_t) socket);
	close((intptr_t) socket);
}

void processRequest(int fd) {
  	//Buffer used to store the client request
	char req[MAXHEAD+1];
	int reqLength = 0;
	int n;

  	//Currently character read
	unsigned char newChar;

  	//Last 3 characters read
	unsigned char lastChar[3];

	//Read until <CRLF><CRLF>
	while ( reqLength < MAXHEAD &&
		( n = read( fd, &newChar, sizeof(newChar) ) ) > 0 ) {

		if ( newChar == '\012' && lastChar[0] == '\015' &&
			lastChar[1] == '\012' && lastChar[2] == '\015') {
			break;
		}

		req[reqLength] = newChar;
		reqLength++;

		lastChar[2] = lastChar[1];
		lastChar[1] = lastChar[0];
		lastChar[0] = newChar;
	}

  	//Add null character at the end of the string
	req[reqLength] = '\0';

	printf("%s\n", req);
	//Make sure request is GET
	char * token = strtok(req, " ");
	if (!token) {
		write(fd, errorHeader, strlen(errorHeader));
		write(fd, errorPage, strlen(errorPage));
		return;
	}
	if (strcmp(token, "GET")) {
		write(fd, errorHeader, strlen(errorHeader));
		write(fd, errorPage, strlen(errorPage));
		return;
	}

	//reqFile is document requested
	char * reqFile = strtok(NULL, " ");
	char * basePath = (char *) "http-root-dir/htdocs";
	char * path;
	char relPath[MAXPATH];

	strcpy(relPath, basePath);
	strcat(relPath, reqFile);
	//printf("%s\n", relPath);

	//Default document: index.html
	if (!strcmp(reqFile, "/")) {
		strcat(relPath, "index.html");
	}

	char actualPath[MAXPATH];
	path = realpath(relPath, actualPath);
	//printf("Full Path: %s\n", path);

	if (!path) {
		write(fd, errorHeader, strlen(errorHeader));
		write(fd, errorPage, strlen(errorPage));
		return;
	}

	//Prevent .. walking
	char baseExpPath[MAXPATH];
	realpath("http-root-dir", baseExpPath);
	size_t baselen = strlen(baseExpPath);
	size_t pathlen = strlen(path);
    if (pathlen < baselen ||
    	strncmp(baseExpPath, path, baselen)) {
    	write(fd, errorHeader, strlen(errorHeader));
		write(fd, errorPage, strlen(errorPage));
		return;
    }

	FILE * fp = fopen(path, "r");

	char * contentType = setContentType(path);

	char header[MAXHEAD];
	sprintf(header, "%sContent-Type: %s\n\n", successHeader, contentType);
	write(fd, header, strlen(header));
	free(contentType);
	int c;
	if (fp) {
		while ((c = getc(fp)) != EOF)
			write(fd, &c, 1);
		fclose(fp);
	}

  // Send last newline
	const char * newline="\n";
	write(fd, newline, strlen(newline));

}

char * setContentType(char * path) {
	//Find extension
	char * tmp = path;
	while (*tmp)
		tmp++;
	while (tmp > path && *tmp != '.') {
		tmp--;
	}

	char * extension = strdup(tmp);
	tmp = NULL;

	//Set Content-Type
	char * contentType;
	if (!strcmp(extension, ".html")) {
		contentType = strdup("text/html");
	}

	else if (!strcmp(extension, ".gif")) {
		contentType = strdup("image/gif");
	}

	else
		contentType = strdup("text/plain");

	free(extension);
	return contentType;
}

void poolSlave(void * masterSocket) {
	printf("thread created");
	while (1) {
		//Accept incoming connections
		struct sockaddr_in clientIPAddress;
		int alen = sizeof( clientIPAddress );
		pthread_mutex_lock(&mutex);
		int slaveSocket = accept((intptr_t)masterSocket,
			(struct sockaddr *)&clientIPAddress,
			(socklen_t*)&alen);
		pthread_mutex_unlock(&mutex);
		if (slaveSocket < 0) {
			perror( "accept" );
			exit( -1 );
		}
		processRequest((intptr_t) slaveSocket);
		printf("close socket\n");
		shutdown((intptr_t) socket, SHUT_RDWR);
		close((intptr_t) socket);
	}
}

void sigChldHandler(int sig) {
	int pid = wait3(NULL, WNOHANG, NULL);
}

void sigPipeHandler(int sig) {
	perror("PIPE Error");
}
