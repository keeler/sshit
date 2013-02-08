#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <signal.h>
#include <syslog.h>
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

#define PORT_NUMBER 12346
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

	openlog( "sshitd", 0, LOG_USER );
	
	// Child process becomes the daemon
	umask( 0 );
	
	pid_t sid = setsid();
	if( sid < 0 )
	{
		syslog( LOG_INFO, "%s", "setsid() failure" );
		exit( EXIT_FAILURE );
	}

	if( chdir( getenv( "HOME" ) ) < 0 )
	{
		syslog( LOG_INFO, "%s", "chdir() failure" );
		exit( EXIT_FAILURE );
	}
	
	close( STDIN_FILENO );
	close( STDOUT_FILENO );
	close( STDERR_FILENO );

	syslog( LOG_INFO, "%s", "daemon setup complete" );

	int handshake_socket;
	struct sockaddr_in myaddr;
	struct sigaction sa;

	handshake_socket = socket( PF_INET, SOCK_STREAM, 0 );
	if( handshake_socket == -1 )
	{
		syslog( LOG_INFO, "%s", "socket() failure" );
		exit( EXIT_FAILURE );
	}
	else
	{
		syslog( LOG_INFO, "%s", "socket() success" );
	}

	int yes = 1;
	if( setsockopt( handshake_socket, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof( int ) ) == -1 )
	{
		syslog( LOG_INFO, "%s", "setsockopt() failure" );
		close( handshake_socket );
		exit( EXIT_FAILURE );
	}
	else
	{
		syslog( LOG_INFO, "%s", "setsockopt() success" );
	}

	myaddr.sin_family = AF_INET; 				/* Host byte order */
	myaddr.sin_port = htons( PORT_NUMBER );			/* Network byte order */
	myaddr.sin_addr.s_addr = htonl( INADDR_ANY ); 		/* Get my IP address */
	memset( &( myaddr.sin_zero ), '\0', 8 ); 			/* Zero it out */

	/* Bind it to your IP address and a suitable port number. */
	if( bind( handshake_socket, (struct sockaddr *)&myaddr, sizeof( struct sockaddr ) ) == -1 )
	{
		syslog( LOG_INFO, "%s", "bind() failure" );
		close( handshake_socket );
		exit( EXIT_FAILURE );
	}
	else
	{
		syslog( LOG_INFO, "%s", "bind() success" );
	}

	if( listen( handshake_socket, 10 ) == -1 )
	{
		syslog( LOG_INFO, "%s", "listen() failure" );
		close( handshake_socket );
		exit( EXIT_FAILURE );
	}
	else
	{
		syslog( LOG_INFO, "%s", "listen() success" );
	}

	// Take care of zombie processes.
	sa.sa_handler = sigchld_handler;
	sigemptyset( &sa.sa_mask );
	sa.sa_flags = SA_RESTART;
	if( sigaction( SIGCHLD, &sa, NULL ) == -1 )
	{
		syslog( LOG_INFO, "%s", "sigaction() failure" );
		close( handshake_socket );
		exit( EXIT_FAILURE );
	}
	else
	{
		syslog( LOG_INFO, "%s", "sigaction() success" );
	}

	while( 1 )
	{
		struct sockaddr_in theiraddr;
		socklen_t theiraddr_size = sizeof( struct sockaddr_in );

		/* Accept the incoming connection request. */
		int connection_socket = accept( handshake_socket, (struct sockaddr *)&theiraddr, &theiraddr_size );
		if( connection_socket == -1 )
		{
			syslog( LOG_INFO, "%s", "accept() failure" );
			continue;
		}
		else
		{
			syslog( LOG_INFO, "%s", "accept() success" );
		}

		if( !fork() )
		{
			char buffer[MAX_BUFFER_SIZE];
			close( handshake_socket );	// Child doesn't need this socket.

			// Get the file name.
			if( recv( connection_socket, buffer, MAX_BUFFER_SIZE, 0 ) == -1 )
			{
				syslog( LOG_INFO, "%s", "recv() failure, filename" );
				close( connection_socket );
				exit( EXIT_FAILURE );
			}
			else
			{
				syslog( LOG_INFO, "%s %s", "Filename", buffer );
			}
			string filename = buffer;

			// Get type of file (text or binary).
			if( recv( connection_socket, buffer, MAX_BUFFER_SIZE, 0 ) == -1 )
			{
				syslog( LOG_INFO, "%s", "recv() failure, file type" );
				close( connection_socket );
				exit( EXIT_FAILURE );
			}
			else
			{
				syslog( LOG_INFO, "%s %s", "File type", buffer );
			}

			// Delete the file if it exists
			if( access( filename.c_str(), F_OK ) == 0 )
			{
				syslog( LOG_INFO, "%s", "unlink() called" );
				unlink( filename.c_str() );
			}

			int size = 0;
			ofstream ofile;
			if( !strcmp( buffer, "text" ) )
			{
				ofile.open( filename.c_str(), ios::out );
				if( ofile.fail() )
				{
					syslog( LOG_INFO, "%s %s %s", "Couldn't open text file", get_current_dir_name(), filename.c_str() );
					close( connection_socket );
					exit( EXIT_FAILURE );
				}
				while( ( size = recv( connection_socket, buffer, MAX_BUFFER_SIZE, 0 ) ) > 0 )
				{
					syslog( LOG_INFO, "%s :: %s", "write", buffer );
					ofile.write( buffer, size );
				}
			}
			else if( !strcmp( buffer, "binary" ) )
			{
				ofile.open( filename.c_str(), ios::out | ios::binary );
				if( ofile.fail() )
				{
					syslog( LOG_INFO, "%s %s", "Couldn't open binary file", filename.c_str() );
					close( connection_socket );
					exit( EXIT_FAILURE );
				}
				while( ( size = recv( connection_socket, buffer, MAX_BUFFER_SIZE, 0 ) ) > 0 )
				{
					syslog( LOG_INFO, "%s :: %s", "write", buffer );
					ofile.write( buffer, size );
				}
			}
			else
			{
				close( connection_socket );
				exit( EXIT_FAILURE );
			}

			ofile.close();
			close( connection_socket );
			exit( EXIT_SUCCESS );
		}

		close( connection_socket );	// Parent should get ready for next transmission.

		sleep( 10 );
	}

	return 0;
}

