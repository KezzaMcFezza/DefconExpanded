#include "systemiv.h"

#include "native_dialog.h"

//
// This will fail if SDL changes their struct layout

typedef char NativeDialogFilter_must_match_SDL_DialogFileFilter[( sizeof( NativeDialogFilter ) == sizeof( SDL_DialogFileFilter ) ) ? 1 : -1];

struct DialogUserdata
{
	NativeDialogCallback callback;
	void *userdata;
};

static void invoke_cb_and_free( void *ud, const char *const *filelist, int filter, int passFilter )
{
	if ( !ud )
	{
		return;
	}

	DialogUserdata *pack = (DialogUserdata *)ud;
	if ( pack && pack->callback )
	{
		pack->callback( pack->userdata, filelist, passFilter );
	}

	free( pack );
}

static void SDLCALL sdl_open_file_cb( void *userdata, const char *const *filelist, int filter )
{
	invoke_cb_and_free( userdata, filelist, filter, filter );
}

static void SDLCALL sdl_open_folder_cb( void *userdata, const char *const *filelist, int filter )
{
	(void)filter;
	invoke_cb_and_free( userdata, filelist, -1, -1 );
}

static void SDLCALL sdl_save_file_cb( void *userdata, const char *const *filelist, int filter )
{
	invoke_cb_and_free( userdata, filelist, filter, filter );
}

void NativeShowOpenFileDialog( void *parentWindow, const char *defaultPath,
							   int allowMany, const NativeDialogFilter *filters, int nFilters,
							   NativeDialogCallback callback, void *userdata )
{
	//
	// Callback is required

	if ( !callback )
	{
		return;
	}

	DialogUserdata *pack = (DialogUserdata *)malloc( sizeof( DialogUserdata ) );
	if ( !pack )
	{
		//
		// Out of memory, call callback with NULL to signal error

		callback( userdata, NULL, -1 );
		return;
	}
	pack->callback = callback;
	pack->userdata = userdata;

	SDL_Window *win = (SDL_Window *)parentWindow;
	const SDL_DialogFileFilter *sdlFilters = NULL;
	if ( filters && nFilters > 0 )
	{
		sdlFilters = (const SDL_DialogFileFilter *)filters;
	}

	SDL_ShowOpenFileDialog( sdl_open_file_cb, pack, win, sdlFilters, nFilters, defaultPath, allowMany ? true : false );
}

void NativeShowOpenFolderDialog( void *parentWindow, const char *defaultPath,
								 int allowMany, NativeDialogCallback callback, void *userdata )
{
	if ( !callback )
	{
		return;
	}

	DialogUserdata *pack = (DialogUserdata *)malloc( sizeof( DialogUserdata ) );
	if ( !pack )
	{
		callback( userdata, NULL, -1 );
		return;
	}

	pack->callback = callback;
	pack->userdata = userdata;

	SDL_Window *win = (SDL_Window *)parentWindow;
	SDL_ShowOpenFolderDialog( sdl_open_folder_cb, pack, win, defaultPath, allowMany ? true : false );
}

void NativeShowSaveFileDialog( void *parentWindow, const char *defaultPath,
							   const NativeDialogFilter *filters, int nFilters,
							   NativeDialogCallback callback, void *userdata )
{
	if ( !callback )
	{
		return;
	}

	DialogUserdata *pack = (DialogUserdata *)malloc( sizeof( DialogUserdata ) );
	if ( !pack )
	{
		callback( userdata, NULL, -1 );
		return;
	}

	pack->callback = callback;
	pack->userdata = userdata;

	SDL_Window *win = (SDL_Window *)parentWindow;
	const SDL_DialogFileFilter *sdlFilters = NULL;
	if ( filters && nFilters > 0 )
	{
		sdlFilters = (const SDL_DialogFileFilter *)filters;
	}

	SDL_ShowSaveFileDialog( sdl_save_file_cb, pack, win, sdlFilters, nFilters, defaultPath );
}
