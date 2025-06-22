/*
 *  * DedCon: Dedicated Server for Defcon
 *  *
 *  * Copyright (C) 2007 Manuel Moos
 *  *
 *  */

#include "Console.h"
#include "GameSettings/Settings.h"

#include <iostream>

#ifdef WIN32
#include <io.h>
#include <windows.h>
#else
#include <signal.h>
#endif

#ifdef _WIN32
    #include <windows.h>
    #define sleep(x) Sleep(1000 * (x))  // Sleep in milliseconds on Windows
#else
    #include <unistd.h>
#endif

#include <stdio.h>
#include <fcntl.h>

#include <sstream>

static bool unblocked = false;

// handler for SIGCONT: unblock stdin
#ifndef WIN32
static int stdin_descriptor;
static void SIGCONTHandler( int )
{
    stdin_descriptor=fileno(stdin);
    int flag=fcntl(stdin_descriptor,F_GETFL);
    fcntl(stdin_descriptor,F_SETFL,flag | O_NONBLOCK);
}
#endif

static void UnblockStdin(){
#ifndef WIN32
    // install handler
    if ( !unblocked )
    {
        signal( SIGCONT, &SIGCONTHandler );
    }

    // and unblock stdin
    SIGCONTHandler(0);
#endif

    unblocked = true;
}


#define MAXLINE 1000
static char line_in[MAXLINE+2];
static int currentIn=0;

static void Apply()
{
    // prepare a reader
    SettingsReader reader;
    reader.AddLine( "console", line_in );

    // and execute it
    reader.Apply();

    // transfer any leftover children of the reader to the main reader.
    reader.TransferChildren( SettingsReader::GetSettingsReader() );

    currentIn=0;
}

void Console::Read(){
    if ( !unblocked )
    {
        UnblockStdin();
    }

#ifdef WIN32
    HANDLE stdinhandle = GetStdHandle(STD_INPUT_HANDLE);
    HANDLE stdouthandle = GetStdHandle(STD_OUTPUT_HANDLE);
    bool goon = true;
    while (goon)
    {
        unsigned long reallyread;
        INPUT_RECORD input;
        bool ret=PeekConsoleInput(stdinhandle, &input, 1, &reallyread);
        if (ret && reallyread > 0)
        {
            bool ret=ReadConsoleInput(stdinhandle, &input, 1, &reallyread);
            if (ret && input.EventType == KEY_EVENT)
            {
                char key = input.Event.KeyEvent.uChar.AsciiChar;
                DWORD written=0;

                if (key && input.Event.KeyEvent.bKeyDown)
                {
                    WriteConsole(stdouthandle, &key, 1, &written, NULL);
                    line_in[currentIn] = key;

                    if (key == 13 || currentIn>=MAXLINE-1){
                        line_in[currentIn]='\n';
                        line_in[currentIn+1]='\0';

                        std::cout << "\n";

                        Apply();
                    }
                    else
                        currentIn++;
                }
            }
            //		bool ret=ReadFile(stdinhandle, &line_in[currentIn], 1, &reallyread, NULL);
        }
        else
            goon = false;
    }


#else
    while (read(stdin_descriptor,&line_in[currentIn],1)>0){
        if (line_in[currentIn]=='\n' || currentIn>=MAXLINE-1){
            line_in[currentIn+1]='\0';

            Apply();
        }
        else
            currentIn++;
    }
#endif
}
