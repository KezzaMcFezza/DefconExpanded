#include "systemiv.h"

#ifdef WIN32
#include <io.h>
#include <direct.h>
#include <windows.h>

#else
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <dirent.h>
#include <errno.h>
#endif

#include <stdio.h>
#include <string.h>
#include <stdlib.h> // needed for errno definition
#include <ctype.h>
#include <stdarg.h>
#include <filesystem>
#include <algorithm>

#include "lib/string_utils.h"
#include "filesys_utils.h"

namespace fs = std::filesystem;

#ifdef TARGET_MSVC
#pragma warning( disable : 4996 )
#endif

//*****************************************************************************
// Misc directory and filename functions
//*****************************************************************************


static bool FilterMatch( const char *_filename, const char *_filter )
{
	while ( *_filename && tolower( *_filename ) == tolower( *_filter ) )
	{
		_filename++;
		_filter++;
	}

	if ( tolower( *_filename ) == tolower( *_filter ) )
		return true;

	switch ( *_filter++ )
	{
		case '*':
			while ( *_filename )
			{
				_filename++;
				if ( FilterMatch( _filename, _filter ) )
					return true;
			}
			return false;

		case '?':
			if ( !*_filename )
				return false;
			_filename++;
			return FilterMatch( _filename, _filter );

		default:
			return false;
	}
}

// Finds all the filenames in the specified directory that match the specified
// filter. Directory should be like "data/textures" or "data/textures/".
// Filter can be NULL or "" or "*.bmp" or "map_*" or "map_*.txt"
// Set FullFilename to true if you want results like "data/textures/blah.bmp"
// or false for "blah.bmp"
LList<char *> *ListDirectory( const char *_dir, const char *_filter, bool _fullFilename )
{
	if ( _filter == NULL || _filter[0] == '\0' )
	{
		_filter = "*";
	}

	if ( _dir == NULL || _dir[0] == '\0' )
	{
		_dir = "";
	}

	// Create a DArray for our results
	LList<char *> *result = new LList<char *>();

	// Now add on all files found locally
#ifdef WIN32
	try
	{
		fs::path dirPath( _dir ? _dir : "." );
		if ( !fs::exists( dirPath ) || !fs::is_directory( dirPath ) )
		{
			return result;
		}

		for ( const auto &entry : fs::directory_iterator( dirPath ) )
		{
			if ( entry.is_regular_file() )
			{
				std::string filename = entry.path().filename().string();

				//
				// Skip . and ..

				if ( filename == "." || filename == ".." )
					continue;

				if ( !FilterMatch( filename.c_str(), _filter ) )
					continue;

				char *newname = NULL;
				if ( _fullFilename )
				{
					std::string fullPath = ( _dir ? _dir : "." );
					if ( !fullPath.empty() && fullPath.back() != '/' && fullPath.back() != '\\' )
					{
						fullPath += "/";
					}
					fullPath += filename;
					std::replace( fullPath.begin(), fullPath.end(), '\\', '/' );
					newname = newStr( fullPath.c_str() );
				}
				else
				{
					newname = newStr( filename.c_str() );
				}

				result->PutData( newname );
			}
		}
	}
	catch ( const fs::filesystem_error & )
	{
	}
#else
	DIR *dir = opendir( _dir[0] ? _dir : "." );
	if ( dir == NULL )
		return result;
	for ( struct dirent *entry; ( entry = readdir( dir ) ) != NULL; )
	{
		if ( FilterMatch( entry->d_name, _filter ) )
		{
			char fullname[strlen( _dir ) + strlen( entry->d_name ) + 2];
			sprintf( fullname, "%s%s%s",
					 _dir,
					 _dir[0] ? "/" : "",
					 entry->d_name );
			if ( !IsDirectory( fullname ) )
			{
				result->PutData(
					newStr( _fullFilename ? fullname : entry->d_name ) );
			}
		}
	}
	closedir( dir );
#endif

	return result;
}


LList<char *> *ListSubDirectoryNames( const char *_dir )
{
	LList<char *> *result = new LList<char *>();

#ifdef WIN32
	try
	{
		fs::path dirPath( _dir ? _dir : "." );
		if ( !fs::exists( dirPath ) || !fs::is_directory( dirPath ) )
		{
			return result;
		}

		for ( const auto &entry : fs::directory_iterator( dirPath ) )
		{
			if ( entry.is_directory() )
			{
				std::string dirname = entry.path().filename().string();
				if ( dirname == "." || dirname == ".." )
					continue;

				char *newname = newStr( dirname.c_str() );
				result->PutData( newname );
			}
		}
	}
	catch ( const fs::filesystem_error & )
	{
	}
#else

	DIR *dir = opendir( _dir );
	if ( dir == NULL )
		return result;
	for ( struct dirent *entry; ( entry = readdir( dir ) ) != NULL; )
	{
		if ( entry->d_name[0] == '.' )
			continue;

		char fullname[strlen( _dir ) + strlen( entry->d_name ) + 2];
		sprintf( fullname, "%s%s%s",
				 _dir,
				 _dir[0] ? "/" : "",
				 entry->d_name );

		if ( IsDirectory( fullname ) )
			result->PutData( newStr( entry->d_name ) );
	}
	closedir( dir );

#endif
	return result;
}


bool DoesFileExist( const char *_fullPath )
{
#ifndef WIN32
	std::string path = FindCaseInsensitive( _fullPath );
	FILE *f = fopen( path.c_str(), "r" );
#else
	FILE *f = fopen( _fullPath, "r" );
#endif
	if ( f )
	{
		fclose( f );
		return true;
	}

	return false;
}


#define FILE_PATH_BUFFER_SIZE 256
static char s_filePathBuffer[FILE_PATH_BUFFER_SIZE + 1];

char *ConcatPaths( const char *_firstComponent, ... )
{
	va_list components;
	const char *component;
	char *buffer, *returnBuffer;

	buffer = strdup( _firstComponent );

	va_start( components, _firstComponent );
	while ( ( component = va_arg( components, const char * ) ) != 0 )
	{
		buffer = (char *)realloc( buffer, strlen( buffer ) + 1 + strlen( component ) + 1 );
		strcat( buffer, "/" );
		strcat( buffer, component );
	}
	va_end( components );

	returnBuffer = newStr( buffer );
	free( buffer );
	return returnBuffer;
}


char *GetDirectoryPart( const char *_fullFilePath )
{
	strncpy( s_filePathBuffer, _fullFilePath, FILE_PATH_BUFFER_SIZE );

	char *finalSlash = strrchr( s_filePathBuffer, '/' );
	if ( finalSlash )
	{
		*( finalSlash + 1 ) = '\x0';
		return s_filePathBuffer;
	}

	return NULL;
}


char *GetFilenamePart( const char *_fullFilePath )
{
	const char *filePart = strrchr( _fullFilePath, '/' ) + 1;
	if ( filePart )
	{
		strncpy( s_filePathBuffer, filePart, FILE_PATH_BUFFER_SIZE );
		return s_filePathBuffer;
	}
	return NULL;
}


const char *GetExtensionPart( const char *_fullFilePath )
{
	if ( !strrchr( _fullFilePath, '.' ) )
		return NULL;
	return strrchr( _fullFilePath, '.' ) + 1;
}


char *RemoveExtension( const char *_fullFileName )
{
	strcpy( s_filePathBuffer, _fullFileName );

	char *dot = strrchr( s_filePathBuffer, '.' );
	if ( dot )
		*dot = '\x0';

	return s_filePathBuffer;
}


bool AreFilesIdentical( const char *_name1, const char *_name2 )
{
	FILE *in1 = fopen( _name1, "r" );
	FILE *in2 = fopen( _name2, "r" );
	bool rv = true;
	bool exitNow = false;

	if ( !in1 || !in2 )
	{
		rv = false;
		exitNow = true;
	}

	while ( exitNow == false && !feof( in1 ) && !feof( in2 ) )
	{
		int a = fgetc( in1 );
		int b = fgetc( in2 );
		if ( a != b )
		{
			rv = false;
			exitNow = true;
			break;
		}
	}

	if ( exitNow == false && feof( in1 ) != feof( in2 ) )
	{
		rv = false;
	}

	if ( in1 )
		fclose( in1 );
	if ( in2 )
		fclose( in2 );

	return rv;
}


bool CreateDirectory( const char *_directory )
{
#ifdef WIN32
	int result = _mkdir( _directory );
	if ( result == 0 )
		return true; // Directory was created
	if ( result == -1 && errno == 17 /* EEXIST */ )
		return true; // Directory already exists
	else
		return false;
#else
	struct stat st;
	if ( stat( _directory, &st ) == 0 )
		return S_ISDIR( st.st_mode );
	if ( mkdir( _directory, 0755 ) == 0 )
		return true;
	if ( errno == EEXIST )
		return true;
	return false;
#endif
}

bool CreateDirectoryRecursively( const char *_directory )
{
	if ( strlen( _directory ) == 0 )
		return false;

#if defined(WIN32)
	const char *p;
	char *buffer;
	bool error = false;

	buffer = new char[strlen( _directory ) + 1];
	p = _directory;
	if ( *p == '/' )
		p++;

	p = strchr( p, '/' );
	while ( p != NULL && !error )
	{
		memcpy( buffer, _directory, p - _directory );
		buffer[p - _directory] = '\0';
		error = !CreateDirectory( buffer );
		p = strchr( p + 1, '/' );
	}

	if ( error )
	{
		delete[] buffer;
		return false;
	}
	delete[] buffer;
	return CreateDirectory( _directory );
#else
	try
	{
		return fs::create_directories( fs::path( _directory ) );
	}
	catch ( ... )
	{
		return false;
	}
#endif
}


void DeleteThisFile( const char *_filename )
{
#ifdef WIN32
	DeleteFile( _filename );
#else
	unlink( _filename );
#endif
}


bool IsDirectory( const char *_fullPath )
{
#ifdef WIN32
	DWORD dwAtts = ::GetFileAttributes( _fullPath );
	if ( dwAtts == INVALID_FILE_ATTRIBUTES )
		return false;
	return ( dwAtts & FILE_ATTRIBUTE_DIRECTORY ) != 0;
#else
	struct stat s;
	int rc = stat( _fullPath, &s );
	if ( rc != 0 )
		return false;
	return ( s.st_mode & S_IFDIR );
#endif
}


#ifndef TARGET_OS_LINUX
std::string FindCaseInsensitive( const std::string &_fullPath )
{
	return _fullPath;
}
#else
std::string FindCaseInsensitive( const std::string &_fullPath )
{
	using std::string;

	string pathSoFar;
	string::size_type componentIndex = 0;

	while ( true )
	{
		string::size_type delimiter = _fullPath.find( '/', componentIndex );

		string component =
			delimiter == string::npos
				? _fullPath.substr( componentIndex )
				: _fullPath.substr( componentIndex, delimiter - componentIndex );

		// Search for a match
		LList<char *> *matches = ListDirectory( pathSoFar.c_str(), component.c_str(), true );
		bool found = false;

		if ( matches->Size() > 0 )
		{
			pathSoFar = matches->GetData( 0 );
			found = true;
		}

		matches->EmptyAndDeleteArray();
		delete matches;

		// Failed to find a match, just return the original
		if ( !found )
			return _fullPath;

		// Got to the end of the path, return it
		if ( delimiter == string::npos )
			return pathSoFar;

		componentIndex = delimiter + 1;
	}
}
#endif
