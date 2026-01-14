#include "systemiv.h"

#include "lib/debug/debug_utils.h"
#include "imgui-1.92.5/imgui.h"
#include "text_editor.h"
#include <stdio.h>
#include <string.h>

TextEditor::TextEditor()
	: m_isOpen( false ),
	  m_textBuffer( NULL ),
	  m_bufferSize( 0 ),
	  m_modified( false )
{
	SetName( "Text Editor" );
	m_filePath[0] = '\0';
}


TextEditor::~TextEditor()
{
	if ( m_textBuffer )
	{
		delete[] m_textBuffer;
		m_textBuffer = NULL;
	}
}


bool TextEditor::LoadFile( const char *filePath )
{
	FILE *file = fopen( filePath, "rb" );
	if ( !file )
	{
		AppDebugOut( "Failed to open file: %s\n", filePath );
		return false;
	}

	fseek( file, 0, SEEK_END );
	size_t fileSize = ftell( file );
	fseek( file, 0, SEEK_SET );

	if ( m_textBuffer )
	{
		delete[] m_textBuffer;
	}

	m_bufferSize = fileSize + 4096;
	m_textBuffer = new char[m_bufferSize];
	memset( m_textBuffer, 0, m_bufferSize );

	if ( fileSize > 0 )
	{
		fread( m_textBuffer, 1, fileSize, file );

		//
		// Normalize line endings since ImGui expects Unix line endings

		size_t writePos = 0;
		for ( size_t readPos = 0; readPos < fileSize; readPos++ )
		{
			if ( m_textBuffer[readPos] == '\r' && readPos + 1 < fileSize && m_textBuffer[readPos + 1] == '\n' )
			{
				continue;
			}
			m_textBuffer[writePos++] = m_textBuffer[readPos];
		}
		m_textBuffer[writePos] = '\0';
	}

	fclose( file );

	strncpy( m_filePath, filePath, sizeof( m_filePath ) - 1 );
	m_filePath[sizeof( m_filePath ) - 1] = '\0';
	m_modified = false;
	m_isOpen = true;

	return true;
}


bool TextEditor::SaveFile()
{
	if ( !m_textBuffer || m_filePath[0] == '\0' )
		return false;

	FILE *file = fopen( m_filePath, "wb" );
	if ( !file )
	{
		AppDebugOut( "Failed to save file: %s\n", m_filePath );
		return false;
	}

#ifdef _WIN32

	//
	// Convert \n back to \r\n for Windows

	size_t len = strlen( m_textBuffer );
	size_t newlineCount = 0;
	for ( size_t i = 0; i < len; i++ )
	{
		if ( m_textBuffer[i] == '\n' )
			newlineCount++;
	}

	if ( newlineCount > 0 )
	{
		char *tempBuffer = new char[len + newlineCount + 1];
		size_t writePos = 0;
		for ( size_t readPos = 0; readPos < len; readPos++ )
		{
			if ( m_textBuffer[readPos] == '\n' )
			{
				tempBuffer[writePos++] = '\r';
				tempBuffer[writePos++] = '\n';
			}
			else
			{
				tempBuffer[writePos++] = m_textBuffer[readPos];
			}
		}
		fwrite( tempBuffer, 1, writePos, file );
		delete[] tempBuffer;
	}
	else
	{
		fwrite( m_textBuffer, 1, len, file );
	}
#else
	size_t len = strlen( m_textBuffer );
	fwrite( m_textBuffer, 1, len, file );
#endif

	fclose( file );

	m_modified = false;
	AppDebugOut( "Saved file: %s\n", m_filePath );
	return true;
}


void TextEditor::Close()
{
	m_isOpen = false;
	if ( m_textBuffer )
	{
		delete[] m_textBuffer;
		m_textBuffer = NULL;
	}
	m_bufferSize = 0;
	m_modified = false;
	m_filePath[0] = '\0';
}


void TextEditor::Draw()
{
	if ( !m_isOpen || !m_textBuffer )
		return;

	char windowTitle[256];
	snprintf( windowTitle, sizeof( windowTitle ), "Text Editor - %s%s###TextEditor",
			  m_filePath, m_modified ? " *" : "" );

	ImGui::SetNextWindowSize( ImVec2( 800, 600 ), ImGuiCond_FirstUseEver );

	if ( ImGui::Begin( windowTitle, &m_isOpen, ImGuiWindowFlags_MenuBar ) )
	{
		if ( ImGui::BeginMenuBar() )
		{
			if ( ImGui::BeginMenu( "File" ) )
			{
				if ( ImGui::MenuItem( "Save", "Ctrl+S" ) )
				{
					SaveFile();
				}

				if ( ImGui::MenuItem( "Close", "Ctrl+W" ) )
				{
					m_isOpen = false;
				}

				ImGui::EndMenu();
			}
			ImGui::EndMenuBar();
		}

		ImGui::PushFont( ImGui::GetIO().Fonts->Fonts[0] );

		ImGui::SetItemDefaultFocus();

		ImGuiInputTextFlags flags = ImGuiInputTextFlags_AllowTabInput;

		if ( ImGui::InputTextMultiline( "##source", m_textBuffer, m_bufferSize,
										ImVec2( -FLT_MIN, -FLT_MIN ), flags ) )
		{
			m_modified = true;
		}

		if ( ImGui::IsItemActive() )
		{
			ImVec2 rectMin = ImGui::GetItemRectMin();
			ImVec2 rectMax = ImGui::GetItemRectMax();
			SDL_Rect textInputRect;
			textInputRect.x = (int)rectMin.x;
			textInputRect.y = (int)rectMin.y;
			textInputRect.w = (int)( rectMax.x - rectMin.x );
			textInputRect.h = (int)( rectMax.y - rectMin.y );
			SDL_SetTextInputRect( &textInputRect );
		}

		ImGui::PopFont();
	}

	ImGui::End();

	if ( !m_isOpen )
	{
		Close();
	}
}
