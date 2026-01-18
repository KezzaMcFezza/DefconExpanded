#include "systemiv.h"

#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#include "lib/gucci/window_manager.h"

#include "imgui-1.92.5/imgui.h"
#include "debug_console.h"
#include "lib/debug/debug_utils.h"
#include "imgui.h"

static DebugConsole *s_debugConsoleInstance = nullptr;

static void StrTrim( char *s )
{
	if ( !s || !*s )
		return;
	char *str_end = s + strlen( s );
	while ( str_end > s && str_end[-1] == ' ' )
		str_end--;
	*str_end = 0;
}


DebugConsole::DebugConsole()
	: m_isOpen( false ),
	  m_autoScroll( true ),
	  m_wasOpen( false ),
	  m_historyPos( -1 ),
	  m_scrollToBottom( false )
{
	SetName( "Console" );
	m_inputBuf[0] = '\0';
	s_debugConsoleInstance = this;

	AppDebugOutFlushEarlyMessages( this );
}


DebugConsole::~DebugConsole()
{
	Shutdown();

	if ( s_debugConsoleInstance == this )
	{
		s_debugConsoleInstance = nullptr;
	}
}


void DebugConsole::Init()
{
}


void DebugConsole::Shutdown()
{
}


void DebugConsole::Update()
{
}


void DebugConsole::Clear()
{
	m_items.clear();
}


void DebugConsole::AddLog( const char *fmt, ... )
{
	char buf[1024];
	va_list ap;
	va_start( ap, fmt );
	vsnprintf( buf, sizeof( buf ), fmt, ap );
	va_end( ap );

	std::string item = buf;

	//
	// Split into lines if there are newlines

	size_t pos = 0;
	while ( ( pos = item.find( '\n', pos ) ) != std::string::npos )
	{
		m_items.push_back( item.substr( 0, pos ) );

		//
		// Trim leading newline and continue

		item = item.substr( pos + 1 );
		pos = 0;
	}

	//
	// Add remaining line

	if ( !item.empty() )
	{
		m_items.push_back( item );
	}

	//
	// Limit buffer size

	while ( m_items.size() > MAX_BUFFER_SIZE )
	{
		m_items.erase( m_items.begin() );
	}

	if ( m_autoScroll )
	{
		m_scrollToBottom = true;
	}
}


void DebugConsole::Render()
{
	if ( !m_isOpen )
		return;

	Window();

	bool open = m_isOpen;
	if ( !ImGui::Begin( "Console", &open ) )
	{
		ImGui::End();
		m_isOpen = open;
		return;
	}

	if ( !open )
	{
		m_isOpen = false;
	}

	//
	// Filter

	const float footer_height_to_reserve = ImGui::GetStyle().ItemSpacing.y + ImGui::GetFrameHeightWithSpacing();
	ImGui::BeginChild( "ScrollingRegion", ImVec2( 0, -footer_height_to_reserve ), false, ImGuiWindowFlags_HorizontalScrollbar );

	if ( ImGui::BeginPopupContextWindow() )
	{
		if ( ImGui::Selectable( "Clear" ) )
			Clear();
		ImGui::EndPopup();
	}

	//
	// Display items

	ImGui::PushStyleVar( ImGuiStyleVar_ItemSpacing, ImVec2( 4, 1 ) );

	for ( size_t i = 0; i < m_items.size(); i++ )
	{
		const char *item = m_items[i].c_str();
		ImGui::TextUnformatted( item );
	}

	//
	// Auto scroll

	if ( m_scrollToBottom && ( ImGui::GetScrollY() >= ImGui::GetScrollMaxY() || m_autoScroll ) )
		ImGui::SetScrollHereY( 1.0f );
	m_scrollToBottom = false;

	ImGui::PopStyleVar();
	ImGui::EndChild();

	//
	// Input

	ImGui::Separator();

	bool reclaim_focus = false;
	ImGuiInputTextFlags input_text_flags = ImGuiInputTextFlags_EnterReturnsTrue;

	if ( ImGui::InputText( "Input", m_inputBuf, sizeof( m_inputBuf ), input_text_flags ) )
	{
		char *s = m_inputBuf;
		StrTrim( s );
		if ( s[0] )
		{
			m_history.push_back( std::string( s ) );
			if ( m_history.size() > MAX_HISTORY_SIZE )
				m_history.erase( m_history.begin() );
			m_historyPos = -1;

			//
			// For now just echo command back to us

			AddLog( "> %s", s );
			strcpy( s, "" );
		}
		reclaim_focus = true;
	}

	ImGui::SetItemDefaultFocus();
	if ( reclaim_focus )
		ImGui::SetKeyboardFocusHere( -1 );

	//
	// Aligned to the right edge

	float checkboxWidth = 120.0f;
	ImGui::SameLine( ImGui::GetWindowWidth() - checkboxWidth );
	ImGui::Checkbox( "Auto-scroll", &m_autoScroll );

	ImGui::End();
}


void DebugConsole::Window()
{
	ImGuiSetWindowLayout(
		DEBUG_CONSOLE_DEFAULT_X_PERCENT,
		DEBUG_CONSOLE_DEFAULT_Y_PERCENT,
		DEBUG_CONSOLE_DEFAULT_WIDTH_PERCENT,
		DEBUG_CONSOLE_DEFAULT_HEIGHT_PERCENT );
}


DebugConsole *DebugConsole::GetInstance()
{
	return s_debugConsoleInstance;
}
