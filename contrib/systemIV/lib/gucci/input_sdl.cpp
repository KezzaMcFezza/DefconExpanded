#include "systemiv.h"

#include <ctime>
#include <cstring>

#ifndef TARGET_MSVC
#include <iconv.h>
#endif

#include <string>
#include <vector>

#include "lib/debug/debug_utils.h"
#include "lib/eclipse/eclipse.h"
#include "lib/hi_res_time.h"
#include "lib/math/math_utils.h"
#include "lib/preferences.h"
#include "lib/resource/resource.h"
#include "lib/filesys/text_stream_readers.h"
#include "lib/language_table.h"
#include "input_sdl.h"
#include "window_manager.h"

// Uncomment for lots of output
// #define VERBOSE_DEBUG

double g_keyDownTime[KEY_MAX];

// TO DO: Unicode text entry.

// *** Constructor
InputManagerSDL::InputManagerSDL()
	: InputManager(),
	  m_rawRmb( false ),
	  m_emulatedRmb( false )
#ifdef TARGET_OS_LINUX
	  ,
	  m_xineramaOffsetHack( g_preferences->GetInt( "XineramaHack", 1 ) )
#endif // TARGET_OS_LINUX
{
	// Don't let SDL do mouse button emulation for us; we'll do it ourselves if
	// we want it.
	m_shouldEmulateRmb = g_preferences->GetInt( "UseOneButtonMouseModifiers", 0 );

#ifdef TARGET_OS_MACOSX
	// This prevents SDL from modifying Ctrl-Left to Right mouse button
	// and Alt-Left to Middle mouse button.
	SDL_SetHint( SDL_HINT_MAC_CTRL_CLICK_EMULATE_RIGHT_CLICK, "0" );
#endif // TARGET_OS_MACOSX
}


constexpr static int ConvertSDLKeyIdToWin32KeyId( int _keyCode )
{
	int keyCode = (int)_keyCode;

	// Specific mappings
	switch ( keyCode )
	{
		case SDLK_TAB:
		case SDLK_KP_TAB:
			return KEY_TAB;
		case SDLK_LSHIFT:
			return KEY_SHIFT;
		case SDLK_RSHIFT:
			return KEY_SHIFT;
		case SDLK_LCTRL:
			return KEY_CONTROL;
		case SDLK_RCTRL:
			return KEY_CONTROL;
		case SDLK_LALT:
			return KEY_ALT;
		case SDLK_RALT:
			return KEY_ALT;
		case SDLK_LGUI:
			return KEY_META;
		case SDLK_RGUI:
			return KEY_META;
		case SDLK_COMMA:
			return KEY_COMMA;
		case SDLK_PERIOD:
			return KEY_STOP;
		case SDLK_SLASH:
			return KEY_SLASH;
		case SDLK_APOSTROPHE:
			return KEY_QUOTE;
		case SDLK_LEFTBRACKET:
			return KEY_OPENBRACE;
		case SDLK_RIGHTBRACKET:
			return KEY_CLOSEBRACE;
		case SDLK_MINUS:
			return KEY_MINUS;
		case SDLK_EQUALS:
			return KEY_EQUALS;
		case SDLK_SEMICOLON:
			return KEY_COLON;
		case SDLK_LEFT:
			return KEY_LEFT;
		case SDLK_RIGHT:
			return KEY_RIGHT;
		case SDLK_UP:
			return KEY_UP;
		case SDLK_DOWN:
			return KEY_DOWN;
		case SDLK_DELETE:
			return KEY_DEL;
		case SDLK_CAPSLOCK:
			return KEY_CAPSLOCK;
		case SDLK_BACKSLASH:
			return KEY_BACKSLASH;
		case SDLK_GRAVE:
			return KEY_TILDE;
		case SDLK_KP_0:
			return KEY_0_PAD;
		case SDLK_KP_1:
			return KEY_1_PAD;
		case SDLK_KP_2:
			return KEY_2_PAD;
		case SDLK_KP_3:
			return KEY_3_PAD;
		case SDLK_KP_4:
			return KEY_4_PAD;
		case SDLK_KP_5:
			return KEY_5_PAD;
		case SDLK_KP_6:
			return KEY_6_PAD;
		case SDLK_KP_7:
			return KEY_7_PAD;
		case SDLK_KP_8:
			return KEY_8_PAD;
		case SDLK_KP_9:
			return KEY_9_PAD;
		case SDLK_KP_PERIOD:
			return KEY_DEL_PAD;
		case SDLK_KP_DIVIDE:
			return KEY_SLASH_PAD;
		case SDLK_KP_MULTIPLY:
			return KEY_ASTERISK;
		case SDLK_KP_MINUS:
			return KEY_MINUS_PAD;
		case SDLK_KP_PLUS:
			return KEY_PLUS_PAD;
		case SDLK_KP_ENTER:
			return KEY_ENTER;
		case SDLK_KP_EQUALS:
			return KEY_EQUALS;
		case SDLK_F1:
			return KEY_F1;
		case SDLK_F2:
			return KEY_F2;
		case SDLK_F3:
			return KEY_F3;
		case SDLK_F4:
			return KEY_F4;
		case SDLK_F5:
			return KEY_F5;
		case SDLK_F6:
			return KEY_F6;
		case SDLK_F7:
			return KEY_F7;
		case SDLK_F8:
			return KEY_F8;
		case SDLK_F9:
			return KEY_F9;
		case SDLK_F10:
			return KEY_F10;
		case SDLK_F11:
			return KEY_F11;
		case SDLK_F12:
			return KEY_F12;
	}

	if ( keyCode >= 97 && keyCode <= 122 )
		keyCode -= 32; // a-z
	else if ( keyCode >= 282 && keyCode <= 293 )
		keyCode -= 170; // Function keys
	else if ( keyCode >= 8 && keyCode <= 57 )
		keyCode -= 0; // 0-9 + BACKSPACE + TAB
	else
		keyCode = 0;

	return keyCode;
}


constexpr int SignOf( int x )
{
	return ( x >= 0 ) ? 1 : 0;
}


void InputManagerSDL::ResetWindowHandle()
{
}


static void BoundMouseXY( int &_x, int &_y )
{
	/* Keep the mouse inside the window */

	int screenW = g_windowManager->WindowW();
	int screenH = g_windowManager->WindowH();

	if ( _x < 0 )
		_x = 0;
	if ( _y < 0 )
		_y = 0;
	if ( _x >= screenW )
		_x = screenW - 1;
	if ( _y >= screenH )
		_y = screenH - 1;
}


#ifdef TARGET_EMSCRIPTEN
// For WebAssembly, use simple vector to avoid template issues
typedef uint32_t wchar32;
typedef std::vector<wchar32> LString;
#else
typedef uint32_t wchar32;
typedef std::basic_string<wchar32> LString;
#endif

static LString ToUTF32( const char *str /* UTF8 */ )
{
#ifdef TARGET_EMSCRIPTEN
	// Simple WebAssembly implementation - just convert ASCII to wchar32
	// This avoids complex template instantiation issues with std::char_traits
	if ( !str )
		return LString();

	LString result;
	for ( const char *p = str; *p; ++p )
	{
		// Only handle basic ASCII for WebAssembly to avoid template issues
		if ( static_cast<unsigned char>( *p ) < 128 )
		{
			result.push_back( static_cast<wchar32>( *p ) );
		}
	}
	return result;
#elif defined( TARGET_OS_MACOSX )
	// Using iconv works on MacOSX and Linux
	if ( !str )
		return LString();
	size_t strSize = strlen( str );
	if ( strSize == 0 )
		return LString();

	LString convertedString;

	iconv_t cd = iconv_open( "UTF-32LE", "UTF-8" );
	if ( (iconv_t)-1 != cd )
	{
		size_t inBytesLeft = strSize;
		size_t outBytesLeft = strSize * sizeof( wchar32 );
		std::vector<wchar32> buffer( outBytesLeft / sizeof( wchar32 ) );
		char *inBytes = const_cast<char *>( str );
		char *outBytes = (char *)&buffer[0];
		iconv( cd, &inBytes, &inBytesLeft, &outBytes, &outBytesLeft );
		convertedString.assign( buffer.begin(), buffer.end() - outBytesLeft /
																   sizeof( wchar32 ) );
		iconv_close( cd );
	}

	return convertedString;
#elif defined( TARGET_OS_LINUX )
	// Linux implementation using similar iconv approach
	if ( !str )
		return LString();
	size_t strSize = strlen( str );
	if ( strSize == 0 )
		return LString();

	LString convertedString;

	iconv_t cd = iconv_open( "UTF-32LE", "UTF-8" );
	if ( (iconv_t)-1 != cd )
	{
		size_t inBytesLeft = strSize;
		size_t outBytesLeft = strSize * sizeof( wchar32 );
		std::vector<wchar32> buffer( outBytesLeft / sizeof( wchar32 ) );
		char *inBytes = const_cast<char *>( str );
		char *outBytes = (char *)&buffer[0];
		iconv( cd, &inBytes, &inBytesLeft, &outBytes, &outBytesLeft );
		convertedString.assign( buffer.begin(), buffer.end() - outBytesLeft / sizeof( wchar32 ) );
		iconv_close( cd );
	}

	return convertedString;
#elif defined( TARGET_MSVC )

	//
	// windows implementation using Windows API for UTF-8 to UTF-32 conversion

	if ( !str )
		return LString();
	size_t strSize = strlen( str );
	if ( strSize == 0 )
		return LString();

	LString convertedString;

	//
	// first convert UTF-8 to UTF-16 using windows API

	int wideLen = MultiByteToWideChar( CP_UTF8, 0, str, (int)strSize, NULL, 0 );
	if ( wideLen > 0 )
	{
		std::vector<wchar_t> wideBuffer( wideLen );
		MultiByteToWideChar( CP_UTF8, 0, str, (int)strSize, &wideBuffer[0], wideLen );

		//
		// convert UTF-16 to UTF-32

		for ( int i = 0; i < wideLen; ++i )
		{
			wchar32 ch = (wchar32)wideBuffer[i];
			if ( ch > 0xff )
				continue;
			convertedString.push_back( ch );
		}
	}

	return convertedString;
#else
	// Fallback for other platforms or placeholder
	// Return empty string for now
	return LString();
#endif
}


// Returns 0 if the event is handled here, -1 otherwise
int InputManagerSDL::EventHandler( unsigned int message, long long wParam, int lParam, bool _isAReplayedEvent )
{
	SDL_Event *sdlEvent = (SDL_Event *)wParam;

	switch ( message )
	{
		case SDL_EVENT_MOUSE_BUTTON_UP:
		{
			if ( sdlEvent->button.button == SDL_BUTTON_LEFT )
			{
				m_lmb = false;
				m_emulatedRmb = false;
			}
			else if ( sdlEvent->button.button == SDL_BUTTON_MIDDLE )
				m_mmb = false;
			else if ( sdlEvent->button.button == SDL_BUTTON_RIGHT )
				m_rawRmb = false;

			m_rmb = m_shouldEmulateRmb ? ( m_rawRmb || m_emulatedRmb ) : m_rawRmb;

			EclUpdateMouse( m_mouseX, m_mouseY, m_lmb, m_rmb );
			break;
		}

		case SDL_EVENT_MOUSE_BUTTON_DOWN:
		{
			if ( sdlEvent->button.button == SDL_BUTTON_LEFT )
			{
				if ( g_keys[KEY_CONTROL] )
					m_emulatedRmb = true;
				else
					m_lmb = true;
			}
			else if ( sdlEvent->button.button == SDL_BUTTON_MIDDLE )
				m_mmb = true;
			else if ( sdlEvent->button.button == SDL_BUTTON_RIGHT )
				m_rawRmb = true;

			m_rmb = m_shouldEmulateRmb ? ( m_rawRmb || m_emulatedRmb ) : m_rawRmb;

			EclUpdateMouse( m_mouseX, m_mouseY, m_lmb, m_rmb );
			break;
		}

		case SDL_EVENT_MOUSE_WHEEL:
		{
			if ( sdlEvent->wheel.y > 0 )
				m_mouseZ += 3;
			else if ( sdlEvent->wheel.y < 0 )
				m_mouseZ -= 3;
			break;
		}

		case SDL_EVENT_MOUSE_MOTION:
		{
			if ( g_windowManager->Captured() )
			{

				int xrel = sdlEvent->motion.xrel;
				int yrel = sdlEvent->motion.yrel;

				m_mouseRelX += xrel;
				m_mouseRelY += yrel;

				m_mouseX += xrel;
				m_mouseY += yrel;

#ifdef VERBOSE_DEBUG
				AppDebugOut( "Mouse captured (%d,%d) - Rel (%d, %d) - Adjusted (%d, %d)\n",
							 sdlEvent->motion.x, sdlEvent->motion.y,
							 sdlEvent->motion.xrel, sdlEvent->motion.yrel,
							 xrel, yrel );
#endif // VERBOSE_DEBUG

				BoundMouseXY( m_mouseX, m_mouseY );
			}
			else
			{
#ifdef VERBOSE_DEBUG
				AppDebugOut( "Mouse uncaptured (%d,%d) - Rel (%d, %d)\n",
							 sdlEvent->motion.x, sdlEvent->motion.y,
							 sdlEvent->motion.xrel, sdlEvent->motion.yrel );
#endif // VERBOSE_DEBUG

				//
				// Convert physical coordinates to logical coordinates
				// Physical mouse coordinates need to be scaled to match logical resolution

				float scaleX = (float)g_windowManager->WindowW() / (float)g_windowManager->PhysicalWindowW();
				float scaleY = (float)g_windowManager->WindowH() / (float)g_windowManager->PhysicalWindowH();

				m_mouseX = (int)( sdlEvent->motion.x * scaleX );
				m_mouseY = (int)( sdlEvent->motion.y * scaleY );
			}

			EclUpdateMouse( m_mouseX, m_mouseY, m_lmb, m_rmb );
			break;
		}

		case SDL_EVENT_KEY_UP:
		{
			int keyCode = ConvertSDLKeyIdToWin32KeyId( sdlEvent->key.key );
			AppDebugAssert( keyCode >= 0 && keyCode < KEY_MAX );
			if ( g_keys[keyCode] != 0 )
			{
				m_numKeysPressed--;
				g_keys[keyCode] = 0;
			}

			break;
		}

		case SDL_EVENT_KEY_DOWN:
		{
			int keyCode = ConvertSDLKeyIdToWin32KeyId( sdlEvent->key.key );
			AppDebugAssert( keyCode >= 0 && keyCode < KEY_MAX );
			if ( g_keys[keyCode] != 1 )
			{
				m_keyNewDeltas[keyCode] = 1;
				m_numKeysPressed++;
				g_keys[keyCode] = 1;
				g_keyDownTime[keyCode] = GetHighResTime();
			}

			EclUpdateKeyboard( keyCode, false, false, false, 0 );

			break;
		}
		case SDL_EVENT_TEXT_INPUT:
		{
			float x, y;
			SDL_GetMouseState( &x, &y );

			LString text = ToUTF32( sdlEvent->text.text );

			// Defcon doesn't really support unicode, so we're going
			// to get some funny results as we add UTF8 encoded
			// code points to our text entries...

			/* Add new text onto the end of our text */
			for ( size_t i = 0; i < text.size(); ++i )
			{
				// Keyboard Handler only supports unsigned shorts
				// for character input.
				if ( text[i] > 0xff )
					continue;

				EclUpdateKeyboard( 0, false, false, false, (unsigned char)text[i] );
			}
			break;
		}
		case SDL_EVENT_WINDOW_FOCUS_LOST:
		{
			m_windowHasFocus = false;
			break;
		}

		case SDL_EVENT_WINDOW_FOCUS_GAINED:
		{
			m_windowHasFocus = true;
			break;
		}

		case SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED:
		case SDL_EVENT_WINDOW_RESIZED:
		case SDL_EVENT_WINDOW_MINIMIZED:
		case SDL_EVENT_WINDOW_MAXIMIZED:
		case SDL_EVENT_WINDOW_RESTORED:
			return 0;

		case SDL_EVENT_QUIT:
		{
			m_quitRequested = true;
			break;
		}

		default:
			return -1;
	}

	return 0;
}


void InputManagerSDL::WaitEvent()
{
	// Wait for some kind of event.
	SDL_Event event;
	SDL_WaitEvent( &event );
	SDL_PushEvent( &event );
}


// Alt Tab issues are Windows Specific
void InputManagerSDL::BindAltTab()
{
	m_altTabBound = true;
}


void InputManagerSDL::UnbindAltTab()
{
}
