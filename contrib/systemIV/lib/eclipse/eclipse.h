
/*
 *          ECLIPSE
 *        version 2.0
 *
 *
 *  Generic Interface Library
 *
 */

#ifndef _included_eclipse_h
#define _included_eclipse_h

#include <stddef.h>  
#include "eclwindow.h"
#include "eclbutton.h"


// ============================================================================
// High level management

void        EclInitialise                   ( int screenW, int screenH );
void        EclShutdown                     ();

void        EclUpdateMouse                  ( int mouseX, int mouseY, bool lmb, bool rmb );
void        EclUpdateKeyboard               ( int keyCode, bool shift, bool ctrl, bool alt, unsigned char ascii );
void        EclRender                       ();
void        EclUpdate                       ();


// ============================================================================
// Window management


void        EclRegisterWindow               ( EclWindow *window, EclWindow *parent=NULL );
void        EclRemoveWindow                 ( const char *name );
void        EclRemoveAllWindows             ();
void		EclRemoveAllWindowsExclude		( const char *exclude );

void        EclBringWindowToFront           ( const char *name );
void        EclSetWindowPosition            ( const char *name, int x, int y );
void        EclSetWindowSize                ( const char *name, int w, int h );

int         EclGetWindowIndex               ( const char *name );                                             // -1 = failure
EclWindow   *EclGetWindow                   ( const char *name );
EclWindow   *EclGetWindow                   ( int x, int y );    
EclWindow   *EclGetWindow                   ();                                                         // get current focussed window                                       

void        EclMaximiseWindow               ( const char *name );
void        EclUnMaximise                   ();

const char        *EclGetCurrentButton            ();
const char        *EclGetCurrentClickedButton     ();

const char        *EclGetCurrentFocus             ();
void        EclSetCurrentFocus              ( const char *name );

void		EclGrabButton					();
void		EclReleaseButton				();

LList       <EclWindow *> *EclGetWindows    ();


// ============================================================================
// Mouse control

bool        EclMouseInWindow                ( EclWindow *window );
bool        EclMouseInButton                ( EclWindow *window, EclButton *button );

enum
{
    EclMouseStatus_NoWindow,
    EclMouseStatus_InWindow,
    EclMouseStatus_Draggable,
    EclMouseStatus_ResizeH,
    EclMouseStatus_ResizeV,
    EclMouseStatus_ResizeHV
};

int         EclMouseStatus                  ();                                                         


// ============================================================================
// Other shit


void        EclRegisterTooltipCallback      ( void (*_callback) (EclWindow *, EclButton *, float) );
const char        *EclGenerateUniqueWindowName    ( const char *name );                                             // In static mem (don't delete!)


#endif
