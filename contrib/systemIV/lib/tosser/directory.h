
/*
 * =========
 * DIRECTORY
 * =========
 *
 * A registry like data structure that can store arbitrary
 * data in a hierarchical system.
 *
 * Can be easily converted into bytestreams for network use.
 *
 */

#ifndef _included_genericdirectory_h
#define _included_genericdirectory_h

#include <iostream>

#include "lib/math/fixed.h"
#include "lib/tosser/darray.h"
#include "lib/tosser/llist.h"

class DirectoryData;

class Directory
{
public:
    char        *m_name;
    DArray      <Directory *>       m_subDirectories;
    DArray      <DirectoryData *>   m_data;

public:
    Directory();
    Directory( const Directory &_copyMe );
    ~Directory();

    void SetName ( const char *newName );

    //
    // Basic data types 

    void    CreateData      ( const char *dataName, int   value );
    void    CreateData      ( const char *dataName, float value );
    void	CreateData		( const char *dataName, Fixed value );
    void    CreateData      ( const char *dataName, unsigned char value );
    void    CreateData      ( const char *dataName, char  value );
    void    CreateData      ( const char *dataName, const char *value );
    void    CreateData      ( const char *dataName, bool  value );
    void    CreateData      ( const char *dataName, void *data, int dataLen );

    int             GetDataInt          ( const char *dataName ) const;                 // These are safe read functions.
    float           GetDataFloat        ( const char *dataName ) const;                 // All return some value.
    Fixed			GetDataFixed		( const char *dataName ) const;
    unsigned char   GetDataUChar        ( const char *dataName ) const;
    char            GetDataChar         ( const char *dataName ) const;
    char            *GetDataString      ( const char *dataName ) const;                       // You should strdup this
    bool            GetDataBool         ( const char *dataName ) const;
    void            *GetDataVoid        ( const char *dataName, int *_dataLen ) const;

    void    RemoveData      ( const char *dataName );
    bool    HasData         ( const char *dataName, int _mustBeType =-1 ) const;


    //
    // Tosser data types

    void    CreateData      ( const char *dataName, DArray    <int> *darray );
    void    CreateData      ( const char *dataName, LList     <int> *llist );
    void    CreateData      ( const char *dataName, LList     <Directory *> *llist );

    void    GetDataDArray   ( const char *dataName, DArray    <int> *darray ) const;
    void    GetDataLList    ( const char *dataName, LList     <int> *llist ) const;
    void    GetDataLList    ( const char *dataName, LList     <Directory *> *llist ) const;

    
    //
    // Low level interface stuff

    Directory       *GetDirectory       ( const char *dirName )  const;
    DirectoryData   *GetData            ( const char *dataName )  const;
    Directory       *AddDirectory       ( const char *dirName );                        // Will create all neccesary subdirs recursivly
    void             AddDirectory       ( Directory *dir );
    void             RemoveDirectory    ( const char *dirName );                        // Directory is NOT deleted
    void             CopyData           ( const Directory *dir,
                                          bool overWrite = false,                       // Overwrite existing data if found
                                          bool directories = true);                     // Copy subdirs as well

    //
    // Saving / Loading to streams

    bool Read  ( std::istream &input );                                                      // returns false if an error occurred while reading
    void Write ( std::ostream &output ) const;

    bool Read   ( const char *input, int length );
    char *Write ( int &length ) const ;                                                     // Creates new string

    static void WriteDynamicString ( std::ostream &output, char *string );   				// Works with NULL
    static char *ReadDynamicString ( std::istream &input );									// Assigns space for string
    
    static void WritePackedInt     ( std::ostream &output, int _value );
    static int  ReadPackedInt      ( std::istream &input );

    static void WriteVoidData      ( std::ostream &output, void *data, int dataLen );       
    static void *ReadVoidData      ( std::istream &input, int *dataLen );				    // Assigns space for data


    //
    // Other 

    void DebugPrint ( int indent ) const;
    void WriteXML ( std::ostream &o, int indent = 0 ) const;

    int  GetByteSize() const;

    static char     *GetFirstSubDir     ( const char *dirName );                      // eg returns "bob" from "bob/hello/poo". Caller must delete.
    static const char *GetOtherSubDirs    ( const char *dirName );                      // eg returns "hello/poo" from above.  Doesn't create new data.

};



/*
 * ==============
 * DIRECTORY DATA
 * ==============
 */

#define DIRECTORY_TYPE_UNKNOWN  0
#define DIRECTORY_TYPE_INT      1
#define DIRECTORY_TYPE_FLOAT    2
#define DIRECTORY_TYPE_CHAR     3
#define DIRECTORY_TYPE_STRING   4
#define DIRECTORY_TYPE_BOOL     5
#define DIRECTORY_TYPE_VOID     6
#define DIRECTORY_TYPE_FIXED    7

#define DIRECTORY_SAFEINT        -1
#define DIRECTORY_SAFEFLOAT      -1.0f
#define DIRECTORY_SAFECHAR       '?'
#define DIRECTORY_SAFESTRING     "[SAFESTRING]"
#define DIRECTORY_SAFEBOOL       false


class DirectoryData
{
public:
    char    *m_name;
    int     m_type;
    
    int     m_int;                  // type 1
    float   m_float;                // type 2
    char    m_char;                 // type 3
    char    *m_string;              // type 4
    bool    m_bool;                 // type 5
    char    *m_void;                // type 6  
	
#ifdef FLOAT_NUMERICS
	double  m_fixed;                // type 7 (stored as raw double to guarantee memory representation)
#elif defined(FIXED64_NUMERICS)
	long long m_fixed;
#endif
    int     m_voidLen;

public:
    DirectoryData();
    ~DirectoryData();

    void SetName ( const char *newName );
    void SetData ( int newData );
    void SetData ( float newData );
	void SetData ( Fixed newData );
    void SetData ( char newData );
    void SetData ( const char *newData );
    void SetData ( bool newData );
    void SetData ( const void *newData, int dataLen );

    void SetData ( const DirectoryData *data );

    bool IsInt()    { return (m_type == DIRECTORY_TYPE_INT      ); }
    bool IsFloat()  { return (m_type == DIRECTORY_TYPE_FLOAT    ); }
	bool IsFixed()  { return (m_type == DIRECTORY_TYPE_FIXED    ); }
    bool IsChar()   { return (m_type == DIRECTORY_TYPE_CHAR     ); }
    bool IsString() { return (m_type == DIRECTORY_TYPE_STRING   ); }
    bool IsBool()   { return (m_type == DIRECTORY_TYPE_BOOL     ); }
    bool IsVoid()   { return (m_type == DIRECTORY_TYPE_VOID     ); }

    void DebugPrint ( int indent );
	void WriteXML ( std::ostream &o, int indent = 0 );

    // Saving / Loading to streams

    bool Read  ( std::istream &input );                              // returns false if something went wrong
    void Write ( std::ostream &output ) const;

};


#endif
