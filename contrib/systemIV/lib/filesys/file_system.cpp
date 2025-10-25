//#define _CRT_SECURE_NO_DEPRECATE
#include "lib/universal_include.h"
#include "lib/string_utils.h"

#ifndef NO_UNRAR
#include "unrar/unrar.h"
#endif // NO_UNRAR

#include "file_system.h"
#include "filesys_utils.h"
#include "text_stream_readers.h"
#include "binary_stream_readers.h"
#include "lib/netlib/net_mutex.h"
#include "lib/netlib/net_thread.h"

#ifdef _WIN32
#include <windows.h>
#else
#include <unistd.h>
#endif

FileSystem *g_fileSystem = NULL;


FileSystem::FileSystem()
{
#ifndef NO_UNRAR
	m_archiveMutex = new NetMutex();
	m_loadingThreadCount = 0;
#endif
}


FileSystem::~FileSystem()
{
#ifndef NO_UNRAR
	delete m_archiveMutex;
	m_archiveMutex = NULL;
#endif
}

void FileSystem::ParseArchives( const char *_dir, const char *_filter )
{
    LList<char *> *results = ListDirectory( _dir, _filter, false );
    for( int i = 0; i < results->Size(); ++i )
    {
        char fullFilename[512];
        snprintf( fullFilename, sizeof(fullFilename), "%s%s", _dir, results->GetData( i ) );
        fullFilename[ sizeof(fullFilename) - 1 ] = '\0';
        ParseArchive( fullFilename );
    }
    results->EmptyAndDeleteArray();
    delete results;
}

#ifndef NO_UNRAR
void FileSystem::ParseArchive( const char *_filename )
{
	UncompressedArchive	*mainData = NULL;

    AppDebugOut( "Parsing archive %s...\n", _filename);

	try
	{
		mainData = new UncompressedArchive(_filename,NULL);
        if( mainData && mainData->m_numFiles > 0 )
        {
            AppDebugOut( "Loaded %s successfully\n", _filename );
        }
        else
        {
            AppDebugOut( "Could not find %s\n", _filename );
        }
	}
	catch( ... )
	{
        AppDebugOut( "Failed to load %s\n", _filename );
		return;
	}

	m_archiveMutex->Lock();
    
	for (int i = 0; i < mainData->m_numFiles; ++i)
	{
		MemMappedFile *file = mainData->m_files[i];
		if (file->m_size > 0)
		{
			strlwr(file->m_filename);
			
			// Subsequent archives may override existing resources
			
			MemMappedFile *oldFile = m_archiveFiles.GetData(file->m_filename);
			if (oldFile) 
            {
				m_archiveFiles.RemoveData(file->m_filename);
				delete oldFile;
			}
			
			m_archiveFiles.PutData(file->m_filename, file);
			mainData->m_files[i] = NULL;
		}
	}
	m_archiveMutex->Unlock();

	delete mainData;
}

//
// thread entry point for parallel archive loading

struct ArchiveThreadData
{
	FileSystem *fs;
	char filename[512];
};

#ifdef WIN32
static unsigned long WINAPI ArchiveLoaderThread(void *_data)
#else
static void* ArchiveLoaderThread(void *_data)
#endif
{
	ArchiveThreadData *data = (ArchiveThreadData *)_data;
	
	data->fs->ParseArchive(data->filename);

	data->fs->m_archiveMutex->Lock();
	data->fs->m_loadingThreadCount--;
	data->fs->m_archiveMutex->Unlock();
	
	delete data;

#ifdef WIN32
	return 0;
#else
	return NULL;
#endif
}

//
// load multiple archives in parallel for faster startup
// each thread calls ParseArchive() to load an archive

void FileSystem::ParseArchivesParallel( LList<char *> *_archiveList )
{
	if( !_archiveList || _archiveList->Size() == 0 ) return;

	m_archiveMutex->Lock();
	m_loadingThreadCount = _archiveList->Size();
	m_archiveMutex->Unlock();

	//
	// spawn threads for each archive
    
	for( int i = 0; i < _archiveList->Size(); ++i )
	{
		ArchiveThreadData *data = new ArchiveThreadData();
		data->fs = this;
		snprintf(data->filename, sizeof(data->filename), "%s", _archiveList->GetData(i));
		data->filename[sizeof(data->filename)-1] = '\0';
		
		NetStartThread((NetThreadFunc)ArchiveLoaderThread, data);
	}
	
	//
	// start the loop - continue until all threads are done
    
	while(true)
	{
		m_archiveMutex->Lock();
		int remaining = m_loadingThreadCount;
		m_archiveMutex->Unlock();
		
		if( remaining == 0 ) break;
		
//
// 8ms seems to be the sweetspot for preventing a busy-wait

#ifdef TARGET_MSVC
		Sleep(8);
#else
		usleep(8000);
#endif
	}
}
#endif // NO_UNRAR


TextReader *FileSystem::GetTextReader(const char *_filename)
{
	TextReader *reader = NULL;

    for( int i = 0; i < m_searchPath.Size(); ++i )
    {
        char fullFilename[512];
        sprintf( fullFilename, "%s%s", m_searchPath[i], _filename );
        if( DoesFileExist(fullFilename) )
        {
            reader = new TextFileReader(fullFilename);
            break;
        }
    }

    if( !reader )
    {
        if (DoesFileExist(_filename))
        {
            reader = new TextFileReader(_filename);	    
        }
    }

#ifndef NO_UNRAR
	if( !reader )
	{
		m_archiveMutex->Lock();
		MemMappedFile *mmfile = m_archiveFiles.GetData(_filename);
		m_archiveMutex->Unlock();
		
		if (mmfile) 
		{
			//reader = new UnicodeTextDataReader((char*)mmfile->m_data, mmfile->m_size, _filename);	
			//if (reader && !reader->IsUnicode())
			//{
			//	delete reader;
				reader = new TextDataReader((char*)mmfile->m_data, mmfile->m_size, _filename);
			//}
		}
	}
#endif // NO_UNRAR

	return reader;
}


BinaryReader *FileSystem::GetBinaryReader(const char *_filename)
{
	BinaryReader *reader = NULL;

    for( int i = 0; i < m_searchPath.Size(); ++i )
    {
        char fullFilename[512];
        sprintf( fullFilename, "%s%s", m_searchPath[i], _filename );
        if( DoesFileExist(fullFilename) )
        {
            reader = new BinaryFileReader(fullFilename);
            break;
        }
    }

    if( !reader )
    {
	    if (DoesFileExist(_filename)) 
        {
            reader = new BinaryFileReader(_filename);
        }
    }

#ifndef NO_UNRAR
    if( !reader )
	{
		m_archiveMutex->Lock();
		MemMappedFile *mmfile = m_archiveFiles.GetData(_filename);
		m_archiveMutex->Unlock();
		
		if (mmfile) reader = new BinaryDataReader(mmfile->m_data, mmfile->m_size, _filename);		
	}
#endif // NO_UNRAR

	return reader;
}



int WildCmp(char const *wild, char const *string) 
{
    char const *cp = NULL;
    char const *mp = NULL;

    while ((*string) && (*wild != '*')) 
    {
        if ((*wild != *string) && (*wild != '?'))
        {
            return 0;
        }
        wild++;
        string++;
    }

    while (*string) 
    {
        if (*wild == '*')
        {
            if (!*++wild)
            {
                return 1;
            }
            mp = wild;
            cp = string + 1;
        }
        else if ((*wild == *string) || (*wild == '?'))
        {
            wild++;
            string++;
        }
        else 
        {
            wild = mp;
            string = cp++;
        }
    }

    while (*wild == '*') 
    {
        wild++;
    }
    return !*wild;
}



// The string is copied and entered into llist
// in correct alphabetical order.
// The string wont be added if it is already present.
void OrderedInsert( LList<char *> *_llist, const char *_newString )
{
    bool added = false;

    for( int i = 0; i < _llist->Size()-1; ++i )
    {
        char *thisString = _llist->GetData(i);
        if( strcmp( _newString, thisString ) == 0 )
        {
            // String duplicate
            return;
        }
        if( strcmp( _newString, thisString ) < 0 )
        {
            _llist->PutDataAtIndex( newStr(_newString), i );
            added = true;
            break;
        }
    }

    if( !added )
    {
        _llist->PutDataAtEnd( newStr(_newString) );
    }
}


// Finds all the filenames in the specified directory that match the specified
// filter. Directory should be like "textures" or "textures/".
// Filter can be NULL or "" or "*.bmp" or "map_*" or "map_*.txt"
// Set _longResults to true if you want results like "textures/blah.bmp" 
// or false for "blah.bmp"
LList<char *> *FileSystem::ListArchive(char *_dir, char *_filter, bool fullFilename)
{
    LList<char *> *results = NULL;

    //
    // List the base data directory

    results = ListDirectory(_dir, _filter, fullFilename);


#ifndef NO_UNRAR
    //
    // List the pre-loaded resource files

    if (m_archiveFiles.Size() > 0)
    {
        if(_filter == NULL || _filter[0] == '\0')
        {
            _filter = "*";
        }

        m_archiveMutex->Lock();
        DArray <char *> *unfilteredResults = m_archiveFiles.ConvertIndexToDArray();
        m_archiveMutex->Unlock();

        for (unsigned int i = 0; i < unfilteredResults->Size(); ++i)
        {
            if (!unfilteredResults->ValidIndex(i)) continue;

            char *fullname = unfilteredResults->GetData(i);
            char *dirPart = (char *) GetDirectoryPart( fullname );
            if( stricmp( dirPart, _dir ) == 0 )
            {
                char *filePart = (char *) GetFilenamePart( fullname );
                int result = WildCmp(_filter, filePart);
                if (result != 0)
                {
                    if( fullname )      OrderedInsert(results, filePart);				
                    else   			    OrderedInsert(results, fullname);				
                }
            }
        }
        delete unfilteredResults;
    }
#endif // NO_UNRAR

    return results;
}


void FileSystem::ClearSearchPath()
{
    m_searchPath.EmptyAndDeleteArray();
}


void FileSystem::AddSearchPath( char *_path )
{
    m_searchPath.PutData( newStr( _path ) );
}

