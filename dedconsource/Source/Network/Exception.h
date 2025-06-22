/*
 *  * DedCon: Dedicated Server for Defcon
 *  *
 *  * Copyright (C) 2007 Manuel Moos
 *  *
 *  */

#ifndef DEDCON_Exception_INCLUDED
#define DEDCON_Exception_INCLUDED

#include <string>

//! exception
class Exception{
public:
    Exception();
    Exception( char const * type, char const * reason );
    ~Exception();

    //! do not print error message
    void Silence();
private:
    std::string reason;
};

//! exception to throw on network read errors
class ReadException: public Exception{
public:
    ReadException(){};
    explicit ReadException( char const * reason );
};

//! exception to throw on network write errors
class WriteException: public Exception{
public:
    WriteException(){};
    explicit WriteException( char const * reason );
};

//! exception to throw when the server should quit
class QuitException: public Exception{
public:
    QuitException( int retval ): ret( retval ){};
    
    int ret;
};

#endif // DEDCON_Exception_INCLUDED
