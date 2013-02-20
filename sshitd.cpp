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

void sigchld_handler( int s )
{
	while( waitpid( -1, NULL, WNOHANG ) > 0 );
}

int main()
{
	pid_t pid;

	pid = fork();
	// Error check
	if( pid < 0 )
	{
		cerr << "sshittyd: failed fork()" << endl;
		exit( EXIT_FAILURE );
	}
	// Exit the parent
	else if( pid > 0 )
	{
		exit( EXIT_SUCCESS );
	}

	// Child process becomes the daemon
	umask( 0 );

	pid_t sid = setsid();
	if( sid < 0 )
	{
		exit( EXIT_FAILURE );
	}

	if( chdir( "/" ) < 0 )
	{
		exit( EXIT_FAILURE );
	}

	close( STDIN_FILENO );
	close( STDOUT_FILENO );
	close( STDERR_FILENO );

	int handshake_socket;
	struct addrinfo hints;
	struct addrinfo *servinfo;
	struct sigaction sa;

	memset( &hints, 0, sizeof( hints ) );
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_flags = AI_PASSIVE;

	if( getaddrinfo( NULL, PORT_NUMBER, &hints, &servinfo ) != 0 )
	{
		exit( EXIT_FAILURE );
	}

	// Bind to the first result possible.
	struct addrinfo *p = NULL;
	for( p = servinfo; p != NULL; p = p->ai_next )
	{
		handshake_socket = socket( p->ai_family, p->ai_socktype, p->ai_protocol );
		if( handshake_socket == -1 )
		{
			continue;
		}

		int yes = 1;
		if( setsockopt( handshake_socket, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof( int ) ) == -1 )
		{
			exit( EXIT_FAILURE );
		}

		if( bind( handshake_socket, p->ai_addr, p->ai_addrlen ) == -1 )
		{
			close( handshake_socket );
			continue;
		}

		break;
	}

	if( p == NULL )
	{
		exit( EXIT_FAILURE );
	}

	freeaddrinfo( servinfo );

	if( listen( handshake_socket, 10 ) == -1 )
	{
		exit( EXIT_FAILURE );
	}

	// Take care of zombie processes.
	sa.sa_handler = sigchld_handler;
	sigemptyset( &sa.sa_mask );
	sa.sa_flags = SA_RESTART;
	if( sigaction( SIGCHLD, &sa, NULL ) == -1 )
	{
		exit( EXIT_FAILURE );
	}

	while( 1 )
	{
		struct sockaddr_storage client_addr;
		socklen_t sin_size = sizeof( client_addr );
		int connection_socket = accept( handshake_socket, (struct sockaddr *)&client_addr, &sin_size );
		if( connection_socket == -1 )
		{
			continue;
		}

		if( !fork() )
		{
			char buffer[MAX_BUFFER_SIZE];
			close( handshake_socket );	// Child doesn't need this socket.
			
			// Get the file name.
			if( recv( connection_socket, buffer, MAX_BUFFER_SIZE, 0 ) == -1 )
			{
				exit( EXIT_FAILURE );
			}
			string filename = buffer;

			// Get type of file (text or binary).
			if( recv( connection_socket, buffer, MAX_BUFFER_SIZE, 0 ) == -1 )
			{
				exit( EXIT_FAILURE );
			}

			int size = 0;
			ofstream ofile;
			if( !strcmp( buffer, "text" ) )
			{
				ofile.open( filename.c_str(), ios::out );
				if( ofile.fail() )
				{
					exit( EXIT_FAILURE );
				}
				while( ( size = recv( connection_socket, buffer, MAX_BUFFER_SIZE, 0 ) ) > 0 )
				{
					ofile << buffer;
				}
			}
			else if( !strcmp( buffer, "binary" ) )
			{
				ofile.open( filename.c_str(), ios::out | ios::binary );
				if( ofile.fail() )
				{
					exit( EXIT_FAILURE );
				}
				while( ( size = recv( connection_socket, buffer, MAX_BUFFER_SIZE, 0 ) ) > 0 )
				{
					ofile.write( buffer, size );
				}
			}
			else
			{
				exit( EXIT_FAILURE );
			}

			close( connection_socket );
			exit( EXIT_SUCCESS );
		}

		close( connection_socket );	// Parent should get ready for next transmission.

		sleep( 10 );
	}

	return 0;
}

