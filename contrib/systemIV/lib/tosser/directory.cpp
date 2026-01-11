#include "systemiv.h"

#include <sstream>
#include <algorithm>

#include <stdio.h>
#include <string.h>

#include "lib/string_utils.h"
#include "lib/debug/debug_utils.h"
#include "directory.h"

#define DIRECTORY_MARKERSIZE     1
#define DIRECTORY_MARKERSTART    "<"
#define DIRECTORY_MARKEREND      ">"

#define DIRECTORY_MAXSTRINGLENGTH   10240
#define DIRECTORY_MAXDIRSIZE        1024

#ifndef _DEBUG
#define _DEBUG
#endif

using namespace std;

inline void reverseCopy( char *_dest, const char *_src, size_t count )
{
	_src += count;
	while (count--)
		*(_dest++) = *(--_src);
} 

// Used for ints, floats and doubles, taking into account endianness
template <class T>
istream &readNetworkValue( istream &input, T &v )
{
#ifndef __BIG_ENDIAN__
	return input.read( (char *) &v, sizeof(T) );
#else
	char u[sizeof(T)];
	input.read( u, sizeof(T) );
	
	char *w = (char *) &v;
	reverseCopy(w, u, sizeof(T));
	return input;
#endif
}

template <class T>
ostream &writeNetworkValue( ostream &output, T v )
{
#ifndef __BIG_ENDIAN__
	return output.write( (char *) &v, sizeof(T) );
#else
	const char *u = (const char *) &v;
	char data[sizeof(T)];
	reverseCopy( data, u, sizeof(T) );
	return output.write( data, sizeof(T) );
#endif	
}

Directory::Directory()
{
    SetName("NewDirectory");
}


Directory::Directory( const Directory &_copyMe )
:   m_name(_copyMe.m_name)
{
    CopyData( &_copyMe, false, true );
}


Directory::~Directory()
{
    m_data.EmptyAndDelete();
    m_subDirectories.EmptyAndDelete();    
}


void Directory::SetName ( const char *newName )
{    
    if( newName && strlen(newName) > 0 )
    {
        m_name = newName;
    }
}


Directory *Directory::GetDirectory ( const char *dirName ) const
{
    AppAssert( dirName );

    std::string firstSubDir = GetFirstSubDir ( dirName );
    const char *otherSubDirs = GetOtherSubDirs ( dirName );

    if ( !firstSubDir.empty() )
    {
        Directory *subDir = NULL;

        //
        // Search for that subdirectory

        for ( int i = 0; i < m_subDirectories.Size(); ++i )
        {
            if ( m_subDirectories.ValidIndex(i) )
            {
                Directory *thisDir = m_subDirectories[i];
                AppAssert( thisDir );
                if ( thisDir->m_name == firstSubDir )
                {
                    subDir = thisDir;
                    break;
                }
            }
        }

        if ( !otherSubDirs )
        {
            return subDir;
        }
        else
        {
            return subDir->GetDirectory( otherSubDirs );
        }
    }
    else
    {
        AppReleaseAssert( false, "Error getting directory %s", dirName );
        return NULL;
    }
}


void Directory::RemoveDirectory( const char *dirName )
{
    for( int i = 0; i < m_subDirectories.Size(); ++i )
    {
        if( m_subDirectories.ValidIndex(i) )
        {
            Directory *subDir = m_subDirectories[i];
            if( subDir->m_name == dirName )
            {
                m_subDirectories.RemoveData(i);             
            }
        }
    }
}


Directory *Directory::AddDirectory ( const char *dirName )
{
    AppAssert( dirName );

    std::string firstSubDir = GetFirstSubDir ( dirName );
    const char *otherSubDirs = GetOtherSubDirs ( dirName );

    if ( !firstSubDir.empty() )
    {
        Directory *subDir = NULL;

        //
        // Search for that subdirectory

        for ( int i = 0; i < m_subDirectories.Size(); ++i )
        {
            if ( m_subDirectories.ValidIndex(i) )
            {
                Directory *thisDir = m_subDirectories[i];
                AppAssert( thisDir );
                if ( thisDir->m_name == firstSubDir )
                {
                    subDir = thisDir;
                    break;
                }
            }
        }

        // If the directory doesn't exist, create it and recurse into it

        if ( !subDir )
        {
            subDir = new Directory();
            subDir->SetName( firstSubDir.c_str() );
            m_subDirectories.PutData( subDir );
        }

        //
        // Have we finished building directories?
        // Otherwise, recurse in

        if ( !otherSubDirs )
        {
            return subDir;
        }
        else
        {
            return subDir->AddDirectory( otherSubDirs );
        }

    }
    else
    {
        AppReleaseAssert(false, "Failed to get first SubDir from %s", dirName );
        return NULL;
    }
}


void Directory::AddDirectory ( Directory *dir )
{
    AppAssert( dir );
    m_subDirectories.PutData( dir );
}


void Directory::CopyData ( const Directory *dir, bool overWrite, bool directories )
{
    AppAssert( dir );

    //
    // Copy Data

    for( int i = 0; i < dir->m_data.Size(); ++i )
    {
        if( dir->m_data.ValidIndex(i) )
        {
            DirectoryData *data = dir->m_data[i];
            AppAssert(data);

            DirectoryData *existing = GetData( data->m_name.c_str() );
            if( existing && overWrite )
            {
                existing->SetData( data );            
            }
            else
            {
                DirectoryData *newData = new DirectoryData();
                newData->SetData( data );
                m_data.PutData( newData );
            }
        }
    }

    //
    // Copy subdirectories

    if( directories )
    {
        for( int d = 0; d < dir->m_subDirectories.Size(); ++d )
        {
            if( dir->m_subDirectories.ValidIndex(d) )
            {
                Directory *subDir = dir->m_subDirectories[d];
                AppAssert( subDir );
    
                Directory *newDir = new Directory( *subDir );
                AddDirectory( newDir );
            }
        }
    }
}


std::string Directory::GetFirstSubDir ( const char *dirName )
{
    AppAssert( dirName );

    //
    // Is there a starting slash

    const char *startPoint = dirName;
    if ( dirName[0] == '/' )
    {
        startPoint = (dirName+1);
    }

    //
    // Find the first slash after that

    const char *endPoint = strchr( startPoint, '/' );

    int dirSize;

    if ( endPoint )
    {
        dirSize = (endPoint - startPoint);
    }
    else
    {
        dirSize = (int) strlen(startPoint);
    }

    if ( dirSize == 0 )
    {
        return std::string();
    }
    else
    {
        return std::string(startPoint, dirSize);
    }
}


const char *Directory::GetOtherSubDirs ( const char *dirName )
{
    AppAssert ( dirName );

    //
    // Is there a starting slash
    
    const char *startPoint = dirName;
    if ( dirName[0] == '/' )
    {
        startPoint = (dirName+1);
    }

    //
    // Find the first slash after that

    const char *endPoint = strchr( startPoint, '/' );
    
    if ( endPoint && (endPoint < (startPoint + strlen(startPoint) - 1) ) )
    {
        return (endPoint+1);
    }
    else
    {
        return NULL;
    }
}


void Directory::CreateData ( const char *dataName, int value )
{
    AppAssert ( dataName );
    
    if ( GetData( dataName ) )
    {
        RemoveData( dataName );
    }

    DirectoryData *newData = new DirectoryData();
    newData->SetName( dataName );
    newData->SetData( value );
    m_data.PutData( newData );
}


void Directory::CreateData ( const char *dataName, float value )
{
    AppAssert ( dataName );
    
    if ( GetData( dataName ) )
    {
        RemoveData( dataName );
    }

    DirectoryData *newData = new DirectoryData();
    newData->SetName( dataName );
    newData->SetData( value );
    m_data.PutData( newData );
}


void Directory::CreateData ( const char *dataName, Fixed value )
{
    AppAssert ( dataName );
    
    if ( GetData( dataName ) )
    {
        RemoveData( dataName );
    }

    DirectoryData *newData = new DirectoryData();
    newData->SetName( dataName );
    newData->SetData( value );
    m_data.PutData( newData );
}


void Directory::CreateData( const char *dataName, unsigned char value )
{
    CreateData( dataName, (char) value );
}


void Directory::CreateData ( const char *dataName, char value )
{
    AppAssert ( dataName );
    
    if ( GetData( dataName ) )
    {
        RemoveData( dataName );
    }

    DirectoryData *newData = new DirectoryData();
    newData->SetName( dataName );
    newData->SetData( value );
    m_data.PutData( newData );
}


void Directory::CreateData ( const char *dataName, const char *value )
{
    AppAssert ( dataName );
    
    if ( GetData( dataName ) )
    {
        RemoveData( dataName );
    }

    DirectoryData *newData = new DirectoryData();
    newData->SetName( dataName );
    newData->SetData( value );
    m_data.PutData( newData );
}


void Directory::CreateData ( const char *dataName, bool value )
{
    AppAssert ( dataName );
    
    if ( GetData( dataName ) )
    {
        RemoveData( dataName );
    }

    DirectoryData *newData = new DirectoryData();
    newData->SetName( dataName );
    newData->SetData( value );
    m_data.PutData( newData );
}


void Directory::CreateData( const char *dataName, void *data, int dataLen )
{
    AppAssert( dataName );

    if ( GetData( dataName ) )
    {
        RemoveData( dataName );
    }
    
    DirectoryData *newData = new DirectoryData();
    newData->SetName( dataName );
    newData->SetData( data, dataLen );
    m_data.PutData( newData );
}


int Directory::GetDataInt ( const char *dataName ) const
{
    DirectoryData *data = GetData( dataName );

    if ( data && data->IsInt() )
    {
        return data->m_int;
    }
    else
    {
#ifndef TARGET_EMSCRIPTEN
        AppDebugOut( "Int Data not found : %s\n", dataName );
#endif
        return DIRECTORY_SAFEINT;
    }
}


float Directory::GetDataFloat ( const char *dataName ) const
{
    DirectoryData *data = GetData( dataName );

    if ( data && data->IsFloat() )
    {
        return data->m_float;
    }
    else
    {
        AppDebugOut( "Float Data not found : %s\n", dataName );
        return DIRECTORY_SAFEFLOAT;
    }
}

Fixed Directory::GetDataFixed ( const char *dataName ) const
{
    DirectoryData *data = GetData( dataName );

    if ( data && data->IsFixed() )
    {
#ifdef FLOAT_NUMERICS
        return Fixed::FromDouble(data->m_fixed);
#elif defined(FIXED64_NUMERICS)
		return Fixed::Fixed(data->m_fixed);
#endif
    }
    else
    {
        AppDebugOut( "Fixed Data not found : %s\n", dataName );
        return Fixed(DIRECTORY_SAFEINT);
    }
}

unsigned char Directory::GetDataUChar( const char *dataName ) const
{
    return (unsigned char) GetDataChar(dataName);
}


char Directory::GetDataChar ( const char *dataName ) const
{
    DirectoryData *data = GetData( dataName );

    if ( data && data->IsChar() )
    {
        return data->m_char;
    }
    else
    {
        AppDebugOut( "Char Data not found : %s\n", dataName );
        return DIRECTORY_SAFECHAR;
    }
}


char *Directory::GetDataString ( const char *dataName ) const
{
    DirectoryData *data = GetData( dataName );

    if ( data && data->IsString() )
    {
        return newStr(data->m_string.c_str());
    }
    else
    {
        AppDebugOut( "String Data not found : %s\n", dataName );
        return const_cast<char*>(DIRECTORY_SAFESTRING);
    }
}


bool Directory::GetDataBool ( const char *dataName ) const
{
    DirectoryData *data = GetData( dataName );

    if ( data && data->IsBool() )
    {
        return data->m_bool;
    }
    else
    {
        AppDebugOut( "Bool Data not found : %s\n", dataName );
        return DIRECTORY_SAFEBOOL;
    }
}


const void *Directory::GetDataVoid( const char *dataName, int *_dataLen ) const
{
    DirectoryData *data = GetData( dataName );

    if( data && data->IsVoid() )
    {
        *_dataLen = static_cast<int>(data->m_void.size());
        return data->m_void.data();
    }
    else
    {
        AppDebugOut( "Void data not found : %s\n", dataName );
        *_dataLen = 0;
        return nullptr;
    }
}


DirectoryData *Directory::GetData ( const char *dataName ) const
{
	int dataSize = m_data.Size();
    for ( int i = 0; i < dataSize; ++i )
    {
        if ( m_data.ValidIndex(i) )
        {
            DirectoryData *data = m_data[i];
            AppAssert (data);
            if ( data->m_name == dataName )
            {
                return data;
            }
        }
    }

    return NULL;
}


void Directory::CreateData ( const char *dataName, DArray <int> *darray )
{
    if ( GetData( dataName ) )
    {
        AppReleaseAssert(false, "Attempted to create duplicate data %s at location\n%s", dataName, m_name.c_str());
    }
    else
    {
        Directory *directory = AddDirectory ( dataName );
        AppAssert( directory );
        directory->CreateData( "tID", "DArray<int>" );
        directory->CreateData( "Size", darray->Size() );

        for ( int i = 0; i < darray->Size(); ++i )
        {
            char indexName[16];
            sprintf ( indexName, "[i %d]", i );
            Directory *thisDir = directory->AddDirectory( indexName );

            if ( darray->ValidIndex(i) ) 
            {
                thisDir->CreateData( "[Used]", true );
                thisDir->CreateData( "[Data]", darray->GetData(i) );
            }
            else
            {               
                thisDir->CreateData( "[Used]", false );
            }
        }
    }
}


void Directory::CreateData ( const char *dataName, LList <Directory *> *llist )
{
    if ( GetData( dataName ) )
    {
        AppReleaseAssert(false,"Attempted to create duplicate data %s at location\n%s", dataName, m_name.c_str());
    }
    else
    {
        Directory *directory = AddDirectory ( dataName );
        AppAssert( directory );
        directory->CreateData( "tID", "LList<Directory *>" );
        directory->CreateData( "Size", llist->Size() );

        for ( int i = 0; i < llist->Size(); ++i )
        {
            char indexName[16];
            sprintf ( indexName, "[i %d]", i );
            Directory *newDir = directory->AddDirectory( indexName );
            Directory *obj = llist->GetData(i);
            AppAssert( obj );
            newDir->AddDirectory( obj );
        }
    }
}


void Directory::CreateData ( const char *dataName, LList<int> *llist )
{
    if( GetData( dataName ) )
    {
        AppReleaseAssert(false, "Attempted to create duplicate data %s at location\n%s", dataName, m_name.c_str());
    }
    else
    {
        Directory *directory = AddDirectory ( dataName );
        AppAssert( directory );
        directory->CreateData( "tID", "LList<int>" );
        directory->CreateData( "Size", llist->Size() );

        for ( int i = 0; i < llist->Size(); ++i )
        {
            char indexName[16];
            sprintf ( indexName, "[i %d]", i );
            int data = llist->GetData(i);
            directory->CreateData( indexName, data );
        }
    }
}


void Directory::GetDataDArray ( const char *dataName, DArray<int> *darray ) const
{
    AppAssert( dataName );
    AppAssert( darray );

    //
    // First try to find a directory representing this DArray

    Directory *dir = GetDirectory( dataName );
    if ( !dir )
    {
        AppReleaseAssert(false, "Failed to find DArray named %s", dataName );
        return;
    }

    //
    // Get info about the DArray

    char *tID = dir->GetDataString("tID");
    int size = dir->GetDataInt("Size");
    if ( strcmp(tID, "DArray<int>") != 0 || size == DIRECTORY_SAFEINT)
    {
        AppDebugOut("Found object %s but it doesn't appear to be a DArray\n" );
    }

    //
    // Iterate through it, loading data
    
    darray->SetSize( size );
    for ( int i = 0; i < size; ++i )
    {
        char indexName[16];
        sprintf ( indexName, "[i %d]", i );
        Directory *thisIndex = dir->GetDirectory( indexName );
        if ( !thisIndex )
        {
            AppDebugOut("Error loading DArray %s : index %d is missing\n", dataName, i );
        }
        else
        {
            bool used = thisIndex->GetDataBool("[Used]");
            
            if ( !used )
            {
                if ( darray->ValidIndex(i) )
                    darray->RemoveData(i);
            }
            else
            {
                int theData = thisIndex->GetDataInt("[Data]");
                darray->PutData( theData, i );
            }
        }
    }
}


void Directory::GetDataLList ( const char *dataName, LList<Directory *> *llist ) const
{
    AppAssert( dataName );
    AppAssert( llist );

    //
    // First try to find a directory representing this LList

    Directory *dir = GetDirectory( dataName );
    if ( !dir )
    {
        AppDebugOut("Failed to find LList named %s\n", dataName);
        return;
    }

    //
    // Get info about the LList

    char *tID = dir->GetDataString("tID");
    int size = dir->GetDataInt("Size");
    if ( strcmp(tID, "LList<Directory *>") != 0 || size == DIRECTORY_SAFEINT)
    {
        AppDebugOut("Found object %s but it doesn't appear to be a LList\n");
    }

    //
    // Iterate through it, loading data
    
    for ( int i = 0; i < size; ++i )
    {
        char indexName[16];
        sprintf ( indexName, "[i %d]", i );
        Directory *thisIndex = dir->GetDirectory( indexName );
        if ( !thisIndex )
        {
            AppDebugOut("Error loading LList %s : index %d is missing\n", dataName, i);
        }
        else
        {
            if( thisIndex->m_subDirectories.ValidIndex(0) )
            {
                Directory *theDir = thisIndex->m_subDirectories[0];
                llist->PutData( theDir );
            }
            else
            {
                AppDebugOut("Error loading LList %s : could not find directory in index %d\n", dataName, i);
            }
        }
    }
}


void Directory::GetDataLList( const char *dataName, LList<int> *llist ) const
{
    AppAssert( dataName );
    AppAssert( llist );

    //
    // First try to find a directory representing this LList

    Directory *dir = GetDirectory( dataName );
    if ( !dir )
    {
        AppDebugOut("Failed to find LList %s\n", dataName);
        return;
    }

    //
    // Get info about the LList

    char *tID = dir->GetDataString("tID");
    int size = dir->GetDataInt("Size");
    if ( strcmp(tID, "LList<int>") != 0 || size == DIRECTORY_SAFEINT)
    {
        AppDebugOut("Found object %s but it doesn't appear to be a LList\n");
    }

    //
    // Iterate through it, loading data
    
    for ( int i = 0; i < size; ++i )
    {
        char indexName[16];
        sprintf ( indexName, "[i %d]", i );
        int theData = dir->GetDataInt("indexName");
        llist->PutDataAtEnd( theData );
    }
}


void Directory::RemoveData( const char *dataName )
{
    for( int i = 0; i < m_data.Size(); ++i )
    {
        if( m_data.ValidIndex(i) )
        {
            DirectoryData *data = m_data[i];
            if( data->m_name == dataName )
            {
                m_data.RemoveData(i);                
                delete data;
            }
        }
    }
}


bool Directory::HasData( const char *dataName, int _mustBeType ) const
{
    //
    // Look for a directory first

    Directory *dir = GetDirectory(dataName);
    if( dir && _mustBeType==-1 ) return true;


    //
    // Look for data

    DirectoryData *data = GetData(dataName);
    
    if( !data ) return false;

    if( _mustBeType == -1 ) return true;

    if( _mustBeType == data->m_type ) return true;

    return false;
}

static void WriteIndent( std::ostream &o, int indent )
{
	for ( int t = 0; t < indent; ++t )
		o.put(' ');
}

void Directory::WriteXML ( std::ostream &o, int indent ) const
{
    //
    // Print our name

	WriteIndent(o, indent);
	o << '<' << m_name << ">\n";
	
    //
    // Print our data	

    for ( int j = 0; j < m_data.Size(); ++j )
    {
        if ( m_data.ValidIndex(j) )
        {	
            DirectoryData *data = m_data[j];
            AppAssert( data );
			data->WriteXML( o, indent + 1 );
		}
	}

    //
    // Recurse into subdirs
	
    for ( int i = 0; i < m_subDirectories.Size(); ++i )
    {
        if ( m_subDirectories.ValidIndex(i) )
        {
            Directory *subDir = m_subDirectories[i];
            AppAssert( subDir );
            subDir->WriteXML( o, indent + 1 );        
        }
    }	
	
	o << "</" << m_name << ">\n";
}
	
void Directory::DebugPrint ( int indent ) const
{
    //
    // Print our name

    for ( int t = 0; t < indent; ++t )
    {
        AppDebugOut( "    " );
    }
    AppDebugOut ( "+===" );
    AppDebugOut ( "%s\n", m_name.c_str() );

    //
    // Print our data

    for ( int j = 0; j < m_data.Size(); ++j )
    {
        if ( m_data.ValidIndex(j) )
        {
            DirectoryData *data = m_data[j];
            AppAssert( data );
            data->DebugPrint( indent + 1 );
        }
    }

    //
    // Recurse into subdirs

    for ( int i = 0; i < m_subDirectories.Size(); ++i )
    {
        if ( m_subDirectories.ValidIndex(i) )
        {
            Directory *subDir = m_subDirectories[i];
            AppAssert( subDir );
            subDir->DebugPrint( indent + 1 );        
        }
    }
}


void Directory::WritePackedInt( ostream &output, int _value )
{
    // The first byte can be either:
    // 0 - 254 inclusive : that value           OR    
    // 255 : int follows (4 bytes)

    if( _value >= 0 && _value < 255 )
    {
        unsigned char byteValue = (unsigned char)_value;
        output.write( (char *) &byteValue, sizeof(byteValue) );
    }
    else
    {
        unsigned char code = 255;
        output.write( (char *) &code, sizeof(code) );
		writeNetworkValue(output,_value);
    }    
}


int Directory::ReadPackedInt( istream &input )
{
    unsigned char code;
    input.read( (char *) &code, sizeof(code) );

    if( code == 255 )
    {
        // The next 4 bytes are a full int
        int fullValue;
		readNetworkValue( input, fullValue );
        return fullValue;
    }
    else
    {
        // This is the value
        return (int)code;
    }
}


bool Directory::Read ( istream &input )
{
    // 
    // Start marker

    char marker[DIRECTORY_MARKERSIZE];
    input.read( marker, DIRECTORY_MARKERSIZE );
    if( strncmp( marker, DIRECTORY_MARKERSTART, DIRECTORY_MARKERSIZE ) != 0 )
    {
        return false;
    }

    
    //
    // Our own information

    m_name = ReadDynamicString( input );
    
    //
    // Our data
    // Indexes are not important and can be forgotten

    int numUsed = ReadPackedInt(input);
    
    if( numUsed < 0 || numUsed > DIRECTORY_MAXDIRSIZE )
    {
        return false;
    }

    for ( int d = 0; d < numUsed; ++d )
    {
        DirectoryData *data = new DirectoryData();        
        if (!data->Read( input )) 
		{
			return false;
		}
        m_data.PutData( data );
    }

       
    // 
    // Our subdirectories

    int numSubdirs = ReadPackedInt( input );

    if( numUsed < 0 || numUsed > DIRECTORY_MAXDIRSIZE )
    {
        return false;
    }
    
    for ( int s = 0; s < numSubdirs; ++s )
    {
        Directory *newDir = new Directory();
        if( !newDir->Read( input ) ) 
		{
			return false;
		}
        m_subDirectories.PutData( newDir );
    }

    //
    // End marker

    input.read( marker, DIRECTORY_MARKERSIZE );
    if( strncmp( marker, DIRECTORY_MARKEREND, DIRECTORY_MARKERSIZE ) != 0 )
    {
        return false;
    }


    return true;
}


void Directory::Write ( ostream &output ) const
{
    // 
    // Start marker

    output.write( DIRECTORY_MARKERSTART, DIRECTORY_MARKERSIZE );
    
    //
    // Our own information

    WriteDynamicString( output, m_name );
    
    //
    // Our data
    // Indexes are not important and can be forgotten
    
    int numUsed = m_data.NumUsed();
    WritePackedInt(output, numUsed);
    
    for ( int d = 0; d < m_data.Size(); ++d )
    {
        if ( m_data.ValidIndex(d) ) 
        {
            DirectoryData *data = m_data[d];
            AppAssert( data );
            data->Write( output );
        }
    }
    

    // 
    // Our subdirectories
    // Indexes are not important and can be forgotten

    int numSubDirs = m_subDirectories.NumUsed();
    WritePackedInt(output, numSubDirs);

    for ( int s = 0; s < m_subDirectories.Size(); ++s )
    {
        if ( m_subDirectories.ValidIndex(s) )
        {
            Directory *subDir = m_subDirectories[s];
            AppAssert( subDir );
            subDir->Write( output );
        }
    }

    //
    // End marker

    output.write( DIRECTORY_MARKEREND, DIRECTORY_MARKERSIZE );
}

void HexDumpData( const char *input, int length, int highlight )
{
	const int linewidth = 16;

	for (int i = 0; i < length; i += linewidth ) {
		for (int j = 0; j < linewidth; j++) {
			if (i + j < length) {
				AppDebugOut("%c", (i + j == highlight) ? '[' : ' ');					
				AppDebugOut("%c", isprint(input[i + j ]) ? input[i + j] : ' ');
				AppDebugOut("%c", (i + j == highlight) ? ']' : ' ');				
			}
			else
				AppDebugOut("   ");
		}
		AppDebugOut("\t");

		for (int j = 0; j < linewidth; j++) {
			if (i +j < length) {
				AppDebugOut("%c", (i + j == highlight) ? '[' : ' ');								
				AppDebugOut("%02x", (((unsigned char *) input)[i + j ]) & 0xFF);				
				AppDebugOut("%c", (i + j == highlight) ? ']' : ' ');				
			}
			else
				AppDebugOut("    ");
		}
		AppDebugOut("\n");
	}
}

bool Directory::Read( const char *input, int length )
{
    istringstream inputStream( string(input, length) );
    bool result = Read( inputStream );
	
	if (!result) 
	{
		AppDebugOut( "Failed to parse directory at position %d.\n", (int) inputStream.tellg() );
		AppDebugOut( "Packet length: %d\n", length );
		AppDebugOut( "Packet data:\n" );
		HexDumpData( input, length, inputStream.tellg() );
		AppDebugOut( "\n" );
		
		istringstream inputStream2( string(input, length) );
		Read( inputStream2 );
	}
    return result;
}


char *Directory::Write( int &length ) const
{
    ostringstream outputStream;
    Write( outputStream );
    outputStream << '\x0';
    
    length = outputStream.tellp();

    char *result = new char[length];
    outputStream.str().copy(result, length);

    return result;
}


std::string Directory::ReadDynamicString ( istream &input )
{
	int size = ReadPackedInt(input);

	if ( size == -1 )
    {
		return std::string();
	}
    else if ( size < 0 ||
              size > DIRECTORY_MAXSTRINGLENGTH )
    {
        return std::string(DIRECTORY_SAFESTRING);
    }
	else
    {
		std::string result(size, '\0');
		input.read( &result[0], size );
		return result;
	}
}


void Directory::WriteDynamicString ( ostream &output, const std::string &string )
{
	if ( !string.empty() )
    {
		int size = (int) string.length();
		WritePackedInt(output, size);
        output.write( string.c_str(), size );
	}
	else
    {
		int size = -1;
		WritePackedInt(output, size);
	}
}


int Directory::GetByteSize() const
{
    int result = 0;

    result += (int) m_name.length();
    result += sizeof(m_data);
    result += sizeof(m_subDirectories);

    for( int i = 0; i < m_data.Size(); ++i )
    {
        if( m_data.ValidIndex(i) )
        {
            DirectoryData *data = m_data[i];
            result += sizeof(data);
            result += (int) data->m_name.length();
            result += (int) data->m_string.length();
            result += (int) data->m_void.size();
        }
    }


    for( int i = 0; i < m_subDirectories.Size(); ++i )
    {
        if( m_subDirectories.ValidIndex(i) )
        {
            Directory *subDir = m_subDirectories[i];
            result += subDir->GetByteSize();
        }
    }

    return result;
}


void Directory::WriteVoidData( std::ostream &output, const void *data, int dataLen )
{
    writeNetworkValue( output, dataLen );
    output.write( (const char *)data, dataLen );
}

std::vector<char> Directory::ReadVoidData( std::istream &input )
{
    int dataLen;
    readNetworkValue( input, dataLen );

    std::vector<char> data(dataLen);
    input.read( data.data(), dataLen );

    return data;
}



DirectoryData::DirectoryData()
:   m_type(DIRECTORY_TYPE_UNKNOWN),
    m_int(-1),
    m_float(-1.0),
    m_char('?'),
    m_bool(false)
{
    SetName("NewData" );
}


DirectoryData::~DirectoryData()
{
}


void DirectoryData::SetName ( const char *newName )
{
    if ( newName )
    {
        m_name = newName;
    }
    else
    {
        m_name.clear();
    }
}


void DirectoryData::SetData ( int newData )
{
    m_type = DIRECTORY_TYPE_INT;
    m_int = newData;
}


void DirectoryData::SetData ( float newData )
{
    m_type = DIRECTORY_TYPE_FLOAT;
    m_float = newData;
}


void DirectoryData::SetData ( Fixed newData )
{
    m_type = DIRECTORY_TYPE_FIXED;
#ifdef FLOAT_NUMERICS
    m_fixed = newData.DoubleValue();
#elif defined(FIXED64_NUMERICS)
	m_fixed = newData.m_value;
#endif
}


void DirectoryData::SetData ( char newData )
{
    m_type = DIRECTORY_TYPE_CHAR;
    m_char = newData;
}


void DirectoryData::SetData ( const char *newData )
{
    m_type = DIRECTORY_TYPE_STRING;

    if( newData )
    {
        m_string = newData;
    }
    else
    {
        m_string.clear();
    }
}


void DirectoryData::SetData ( const void *newData, int dataLen )
{
    m_type = DIRECTORY_TYPE_VOID;

    m_void.resize(dataLen);
    if (dataLen > 0)
    {
        memcpy( m_void.data(), newData, dataLen );
    }
}



void DirectoryData::SetData ( bool newData )
{
    m_type = DIRECTORY_TYPE_BOOL;
    m_bool = newData;
}


void DirectoryData::SetData ( const DirectoryData *data )
{
    AppAssert( data );

    m_name = data->m_name;
    m_type = data->m_type;
    m_int = data->m_int;
    m_float = data->m_float;
	m_fixed = data->m_fixed;
    m_char = data->m_char;
    m_bool = data->m_bool;
    m_string = data->m_string;
    m_void = data->m_void;
}

static void WriteXMLString( ostream &o, const char *x )
{
	while (*x) {
		const char *sep = strpbrk( x, "<>" );
		if (sep) {
			int distance = sep - x;
			if (distance) {
				o.write(x, distance);
				x += distance;
			}
			switch (*x) {
				case '<': o << "&lt;"; break;
				case '>': o << "&gt;"; break;
			}
			x++;
		}
		else {
			o << x;
			return;
		}
	}
}

void DirectoryData::WriteXML( ostream &o, int indent )
{
	if (m_type == DIRECTORY_TYPE_UNKNOWN)
		return;

	WriteIndent(o, indent);

	o << '<' << m_name << '>';

	AppDebugAssert(m_type != DIRECTORY_TYPE_FIXED); // fixed case not implemented
    switch ( m_type )
    {
        case DIRECTORY_TYPE_INT  :          o << m_int;            			    break;
        case DIRECTORY_TYPE_FLOAT :         o << m_float;          		        break;
        case DIRECTORY_TYPE_CHAR  :         o << m_char;           				break;
        case DIRECTORY_TYPE_STRING  :       WriteXMLString(o, m_string.c_str());    	break;
        case DIRECTORY_TYPE_BOOL  :         o << (m_bool ? "true" : "false"); 	break;
    }

	o << "</" << m_name << ">\n";
}


void DirectoryData::DebugPrint ( int indent )
{
#ifdef _DEBUG
    for ( int t = 0; t < indent; ++t )
    {
        AppDebugOut( "    " );
    }
    AppDebugOut ( "+---" );
    AppDebugOut ( "%s : ", m_name.c_str() );

    switch ( m_type )
    {
        case DIRECTORY_TYPE_UNKNOWN  :      AppDebugOut ( "[UNASSIGNED]\n" );                               break;
        case DIRECTORY_TYPE_INT  :          AppDebugOut ( "%d\n", m_int );                                  break;
        case DIRECTORY_TYPE_FLOAT :         AppDebugOut ( "%f\n", m_float );                                break;
#ifdef FLOAT_NUMERICS
		case DIRECTORY_TYPE_FIXED :         AppDebugOut ( "%f\n", m_fixed );								break;
#elif defined(FIXED64_NUMERICS)
		case DIRECTORY_TYPE_FIXED :         AppDebugOut ( "%f\n", Fixed::Fixed(m_fixed).DoubleValue() );	break;
#endif
        case DIRECTORY_TYPE_CHAR  :         AppDebugOut ( "(val=%d)\n", (int)m_char );                      break;
        case DIRECTORY_TYPE_STRING  :       AppDebugOut ( "'%s'\n", m_string.c_str() );                             break;
        case DIRECTORY_TYPE_BOOL  :         AppDebugOut ( "[%s]\n", m_bool ? 
                                                            "true" : "false" );                             break;
        case DIRECTORY_TYPE_VOID :
            AppDebugOut( "Raw Data, Length %d bytes", (int)m_void.size() );
            AppDebugOutData( m_void.data(), (int)m_void.size() );
            break;
    }
#else
    AppDebugOut( "Directory::DebugPrint is hashed out\n" );
#endif
}


bool DirectoryData::Read ( istream &input )
{
	if (!input)
		return false;

    m_name = Directory::ReadDynamicString( input );

	if (!input)
		return false;

	m_type = input.get();

    switch ( m_type )
    {
        case    DIRECTORY_TYPE_INT   :   readNetworkValue( input, m_int );						break;
        case    DIRECTORY_TYPE_FLOAT :   readNetworkValue( input, m_float );					break;
		case    DIRECTORY_TYPE_FIXED :   readNetworkValue( input, m_fixed );					break;
        case    DIRECTORY_TYPE_CHAR  :   input.read( (char *) &m_char, sizeof(m_char) );        break;
        case    DIRECTORY_TYPE_BOOL  :   m_bool = input.get();        break;

        case DIRECTORY_TYPE_STRING:
            m_string = Directory::ReadDynamicString( input );
            break;

        case DIRECTORY_TYPE_VOID:
            m_void = Directory::ReadVoidData( input );
            break;

        default:
            return false;
    }


    return true;
}


void DirectoryData::Write ( ostream &output ) const
{
    //
    // Our own information

    Directory::WriteDynamicString( output, m_name );

	output.put((unsigned char) m_type);

    switch ( m_type )
    {
        case    DIRECTORY_TYPE_INT   :   writeNetworkValue( output, m_int );					break;
        case    DIRECTORY_TYPE_FLOAT :   writeNetworkValue( output, m_float );					break;
		case    DIRECTORY_TYPE_FIXED :   writeNetworkValue( output, m_fixed );					break;
        case    DIRECTORY_TYPE_CHAR  :   output.write( (char *) &m_char, sizeof(m_char) );      break;
        case    DIRECTORY_TYPE_BOOL  :   output.put( m_bool ? 1 : 0 );      break;

        case DIRECTORY_TYPE_STRING:
            Directory::WriteDynamicString(output, m_string );
            break;

        case DIRECTORY_TYPE_VOID:
            Directory::WriteVoidData(output, m_void.data(), (int)m_void.size());
            break;
    }

}


