#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <errno.h>
#include <unistd.h>
#include <syslog.h>
#include <string.h>
#include <iostream>
#include <string>
#include <fstream>
using namespace std;

#define PORT_NUMBER "9876"
#define MAX_BUFFER_SIZE 1024

int main( int argc, char **argv )
{
	if( argc != 4 )
	{
		cerr << "Usage: " << argv[0] << " <dest ip> [-t|-b] <filename>" << endl;
		cerr << "    -t :: send text file called <filename>" << endl;
		cerr << "    -b :: send binary file called <filename>" << endl;
		exit( 1 );
	}

	int sockfd;
	struct addrinfo hints;
	struct addrinfo *servinfo;

	memset( &hints, 0, sizeof( hints ) );
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;

	if( getaddrinfo( argv[1], PORT_NUMBER, &hints, &servinfo ) != 0 )
	{
		exit( EXIT_FAILURE );
	}

	// Connect to the first result possible.
	struct addrinfo *p = NULL;
	for( p = servinfo; p != NULL; p = p->ai_next )
	{
		sockfd = socket( p->ai_family, p->ai_socktype, p->ai_protocol );
		if( sockfd == -1 )
		{
			continue;
		}

		if( connect( sockfd, p->ai_addr, p->ai_addrlen ) == -1 )
		{
			continue;
		}

		break;
	}

	if( p == NULL )
	{
		exit( EXIT_FAILURE );
	}

	freeaddrinfo( servinfo );

	ifstream ifile;
	// Now we're connected to the daemon, try to transmit.
	if( strcmp( argv[2], "-t" ) == 0 )
	{
		ifile.open( argv[3], ios::in );
	}
	else if( strcmp( argv[2], "-b" ) == 0 )
	{
		ifile.open( argv[3], ios::in | ios::binary );
	}
	else
	{
		cerr << "\"" << argv[2] << "\" is not a valid file transfer mode. Use -t for a text file, -b for a binary file." << endl;
		exit( EXIT_FAILURE );
	}

	if( ifile.fail() )
	{
		cerr << "Could not open \"" << argv[3] << "\"." << endl;
		exit( EXIT_FAILURE );
	}

	// Send the filename to the remote terminal.
	if( send( sockfd, argv[2], strlen( argv[2] ) + 1, 0 ) == -1 )
	{
		cerr << "Error sending filename." << endl;
		exit( EXIT_FAILURE );
	}

	if( strcmp( argv[2], "-t" ) == 0 )
	{
		char buf[] = "text";
		if( send( sockfd, buf, 5, 0 ) == -1 )
		{
			cerr << "Error sending file type." << endl;
			exit( EXIT_FAILURE );
		}
	}
	else if( strcmp( argv[2], "-b" ) == 0 )
	{
		char buf[] = "binary";
		if( send( sockfd, buf, 7, 0 ) == -1 )
		{
			cerr << "Error sending file type." << endl;
			exit( EXIT_FAILURE );
		}
	}

	ifile.seekg( 0, ios::end );
	int length = ifile.tellg();
	ifile.seekg( 0, ios::beg );

	char *buffer = new char[length];
	ifile.read( buffer, length );
	ifile.close();

	int currPos = 0;
	while( currPos < length )
	{
		int numSent = 0;
		numSent = send( sockfd, &buffer[currPos], MAX_BUFFER_SIZE, 0 );
		if( numSent == -1 )
		{
			cerr << "Error sending data." << endl;
			exit( EXIT_FAILURE );
		}

		currPos += numSent;
	}

	close( sockfd );
	return 0;
}

