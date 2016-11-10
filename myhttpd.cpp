const char * usage =
"                                                               \n"
"daytime-server:                                                \n"
"                                                               \n"
"Simple server program that shows how to use socket calls       \n"
"in the server side.                                            \n"
"                                                               \n"
"To use it in one window type:                                  \n"
"                                                               \n"
"   daytime-server <port>                                       \n"
"                                                               \n"
"Where 1024 < port < 65536.             \n"
"                                                               \n"
"In another window type:                                       \n"
"                                                               \n"
"   telnet <host> <port>                                        \n"
"                                                               \n"
"where <host> is the name of the machine where daytime-server  \n"
"is running. <port> is the port number you used when you run   \n"
"daytime-server.                                               \n"
"                                                               \n"
"Then type your name and return. You will get a greeting and   \n"
"the time of the day.                                          \n"
"                                                               \n";

const char * successHeader = "HTTP/1.0 200 OK\n"
"Content-Type: text/html\n"
"Server: cashburn\n";

const char * head404 = "HTTP/1.0 404 Not Found\n"
"Content-Type: text/html\n"
"Server: cashburn\n";

const char * errorPage = "<!DOCTYPE html><title>Error</title><p><b>Error</b></p>";

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <time.h>

#define MAXPATH 1024

int QueueLength = 5;

// Processes time request
void processTimeRequest( int socket );

int
main( int argc, char ** argv )
{
  // Print usage if not enough arguments
	if ( argc < 2 ) {
		fprintf( stderr, "%s", usage );
		exit( -1 );
	}

  // Get the port from the arguments
	int port = atoi( argv[1] );

  // Set the IP address and port for this server
	struct sockaddr_in serverIPAddress;
	memset( &serverIPAddress, 0, sizeof(serverIPAddress) );
	serverIPAddress.sin_family = AF_INET;
	serverIPAddress.sin_addr.s_addr = INADDR_ANY;
	serverIPAddress.sin_port = htons((u_short) port);

  // Allocate a socket
	int masterSocket =  socket(PF_INET, SOCK_STREAM, 0);
	if ( masterSocket < 0) {
		perror("socket");
		exit( -1 );
	}

  // Set socket options to reuse port. Otherwise we will
  // have to wait about 2 minutes before reusing the sae port number
	int optval = 1;
	int err = setsockopt(masterSocket, SOL_SOCKET, SO_REUSEADDR,
		(char *) &optval, sizeof( int ) );

  // Bind the socket to the IP address and port
	int error = bind( masterSocket,
		(struct sockaddr *)&serverIPAddress,
		sizeof(serverIPAddress) );
	if ( error ) {
		perror("bind");
		exit( -1 );
	}

  // Put socket in listening mode and set the
  // size of the queue of unprocessed connections
	error = listen( masterSocket, QueueLength);
	if ( error ) {
		perror("listen");
		exit( -1 );
	}

	while ( 1 ) {

    // Accept incoming connections
		struct sockaddr_in clientIPAddress;
		int alen = sizeof( clientIPAddress );
		int slaveSocket = accept( masterSocket,
			(struct sockaddr *)&clientIPAddress,
			(socklen_t*)&alen);

		if ( slaveSocket < 0 ) {
			perror( "accept" );
			exit( -1 );
		}

    // Process request.
		processTimeRequest( slaveSocket );

    // Close socket
		close( slaveSocket );
	}

}

void
processTimeRequest( int fd )
{
  // Buffer used to store the name received from the client
	const int MaxReq = 1024;
	char req[ MaxReq + 1 ];
	int reqLength = 0;
	int n;

  // Currently character read
	unsigned char newChar;

  // Last 3 characters read
	unsigned char lastChar[3];
	//unsigned char lastChar = 0;

  // The client should send <name><cr><lf>
  // Read the name of the client character by character until a
  // <CR><LF> is found.
  //

	while ( reqLength < MaxReq &&
		( n = read( fd, &newChar, sizeof(newChar) ) ) > 0 ) {

		//printf("%d\n", newChar);
		if ( newChar == '\012' && lastChar[0] == '\015' &&
			lastChar[1] == '\012' && lastChar[2] == '\015') {
      // Discard previous <CR> from name
			//reqLength--;
			break;
		}

		req[reqLength] = newChar;
		reqLength++;

		lastChar[2] = lastChar[1];
		lastChar[1] = lastChar[0];
		lastChar[0] = newChar;
	}

  // Add null character at the end of the string
	req[reqLength] = 0;

	printf("%s\n", req);
	char * token = strtok(req, " ");
	if (strcmp(token, "GET")) {
		write(fd, head404, strlen(head404));
		write(fd, errorPage, strlen(errorPage));
		return;
	}

	char * reqFile = strtok(NULL, " ");

	char * basePath = (char *) "http-root-dir/htdocs";
	char * path;
	char relPath[MAXPATH];

	strcpy(relPath, basePath);
	strcat(relPath, reqFile);
	printf("%s\n", relPath);
	if (!strcmp(reqFile, "/")) {
		strcat(relPath, "index.html");
		path = strdup(relPath);
	}
	else {
		char actualPath[MAXPATH];
		path = realpath(relPath, actualPath);
		printf("Full Path: %s\n", path);
	}

	if (!path) {
		write(fd, head404, strlen(head404));
		write(fd, errorPage, strlen(errorPage));
		return;
	}

	FILE * fp = fopen(path, "r");

	write(fd, successHeader, strlen(successHeader));

	int c;
	if (fp) {
		while ((c = getc(fp)) != EOF)
			write(fd, &c, 1);
		fclose(fp);
	}
	//write( fd, hi, strlen( hi ) );
	//write( fd, name, strlen( name ) );
	//write( fd, timeIs, strlen( timeIs ) );

  // Send the time of day
	//write(fd, timeString, strlen(timeString));

  // Send last newline
	const char * newline="\n";
	write(fd, newline, strlen(newline));

}
