#include "Codes.h"
#include <string.h>

// decoded: place to store the decoded code
// max: max number of matching first letters between code and hint so far
// code: the code to decode
// hint: a hint which decoding to use
// ID: the current ID to check
// IDCode: the code the ID would translate to
void DeCode( char const * & decoded, int & max, char const * code, char const * hint, char const * ID, char const * IDcode )
{
    // no match, nothing to do
    if ( strcmp( code, IDcode ) )
        return;

    // count matching characters
    int match = 0;
    while( hint && hint[match] && ID[match] && hint[match] == ID[match] )
    {
        match++;
    }

    if ( match > max )
    {
        max = match;
        decoded = ID;
    }
}

#ifdef COMPILE_DEDCON
#include "Codes/defcon.cpp"
#endif

#ifdef COMPILE_DEDWINIA
#include "multiwinia.cpp"
#endif

