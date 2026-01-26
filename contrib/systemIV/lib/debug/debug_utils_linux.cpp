#include "systemiv.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <signal.h>
#include <sys/ucontext.h>
#include <unistd.h>
#include <errno.h>
#include <execinfo.h>

#include "app/app.h"
#include "debug_utils.h"

#ifndef NO_WINDOW_MANAGER
#include "lib/gucci/window_manager.h"
#endif

static char *s_pathToProgram = 0;

void SetupPathToProgram( const char *program )
{
	//
	// Binreloc gives us the full path

	s_pathToProgram = strcpy( new char[strlen( program ) + 1], program );
}


static void fatalSignal( int signum, siginfo_t *siginfo, void *arg )
{
	static char msg[256];
	sprintf( msg, "Got a fatal signal: %d (", signum );

	switch ( signum )
	{
		case SIGSEGV:
			strcat( msg, "SIGSEGV - Segmentation Fault" );
			break;
		case SIGBUS:
			strcat( msg, "SIGBUS - Bus Error" );
			break;
		case SIGFPE:
			strcat( msg, "SIGFPE - Floating Point Exception" );
			break;
		case SIGILL:
			strcat( msg, "SIGILL - Illegal Instruction" );
			break;
		default:
			strcat( msg, "Unknown Signal" );
			break;
	}
	strcat( msg, ")" );

	ucontext_t *u = (ucontext_t *)arg;

	AppGenerateBlackBox( "blackbox.txt", msg );

	fprintf( stderr, "\n\n========================================\n" );
	fprintf( stderr, "FATAL ERROR\n" );
	fprintf( stderr, "========================================\n" );
	fprintf( stderr, "%s\n\n", msg );

	//
	// Print stack trace to stderr

	std::string stackTrace = AppCaptureStackTrace( 2, 32 );
	fprintf( stderr, "%s\n", stackTrace.c_str() );

	fprintf( stderr,
			 "\n\nThe application has unexpectedly encountered a fatal error.\n"
			 "A full description of the error can be found in the file\n"
			 "blackbox.txt\n\n" );

#ifndef NO_WINDOW_MANAGER
	g_windowManager->UncaptureMouse();
	g_windowManager->UnhideMousePointer();
	SDL_Quit();
#endif

	//
	// Re raise signal to get default behavior

	signal( signum, SIG_DFL );
	raise( signum );
}


void SetupMemoryAccessHandlers()
{
	struct sigaction sa;

	sa.sa_sigaction = fatalSignal;
	sa.sa_flags = SA_SIGINFO | SA_RESETHAND;
	sigemptyset( &sa.sa_mask );

	sigaction( SIGSEGV, &sa, 0 );
	sigaction( SIGBUS, &sa, 0 );
	sigaction( SIGFPE, &sa, 0 );
	sigaction( SIGILL, &sa, 0 );
}
