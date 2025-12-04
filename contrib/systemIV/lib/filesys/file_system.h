#ifndef _included_filesystem_h
#define _included_filesystem_h


/*
 * ===========
 * FILE SYSTEM
 * ===========
 *
 *	A file access system designed to allow an app 
 *  to open files for reading/writing.
 *  Will transparently load from compressed archives 
 *  if they are present.
 *
 */

#ifndef NO_UNRAR
class MemMappedFile;
#endif // NO_UNRAR
class TextReader;
class BinaryReader;
class NetMutex;

#include "lib/tosser/btree.h"
#include "lib/tosser/llist.h"



class FileSystem 
{
protected:
#ifndef NO_UNRAR
	BTree <MemMappedFile *>	m_archiveFiles;
#endif // NO_UNRAR
    
public:
#ifndef NO_UNRAR
	NetMutex *m_archiveMutex;
	int m_loadingThreadCount;	
#endif // NO_UNRAR

    LList <char *> m_searchPath;                                                        // Use to set up mods

public:
    FileSystem();
    ~FileSystem();

#ifndef NO_UNRAR
    void            ParseArchive		 ( const char *_filename );                     // Called by each thread inside ParseArchivesParallel
    void            ParseArchives		 ( const char *_dir, const char *_filter );     // Deprecated but still here incase we need it...
	void            ParseArchivesParallel( LList<char *> *_archiveList );	            // Load archives in parallel
#endif // NO_UNRAR

	TextReader		*GetTextReader	    ( const char *_filename );	                    // Caller must delete the TextReader when done
	BinaryReader	*GetBinaryReader    ( const char *_filename );	                    // Caller must delete the BinaryReader when done
    void            UnloadArchiveFile   ( const char *_filename );                      // Free a cached file from memory

    LList<char *>   *ListArchive        (char *_dir, char *_filter, bool fullFilename = true);


    void            ClearSearchPath     ();
    void            AddSearchPath       ( char *_path );                                // Must be added in order
};




extern FileSystem *g_fileSystem;

#endif
