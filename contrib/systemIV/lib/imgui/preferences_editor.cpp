#include "systemiv.h"

#include "lib/debug/debug_utils.h"
#include "lib/preferences.h"
#include "preferences_editor.h"
#include "components/text_editor.h"

PreferencesEdit::PreferencesEdit()
	: m_textEditor( NULL )
{
	SetName( "Preferences Editor" );
	m_textEditor = new TextEditor();
}


PreferencesEdit::~PreferencesEdit()
{
	delete m_textEditor;
	m_textEditor = NULL;
}


void PreferencesEdit::Draw()
{
	if ( m_textEditor )
	{
		m_textEditor->Draw();
	}
}


bool PreferencesEdit::IsOpen() const
{
	return m_textEditor ? m_textEditor->IsOpen() : false;
}


void PreferencesEdit::Open()
{
	if ( m_textEditor )
	{
		const char *prefsPath = GetPreferencesPath();
		if ( prefsPath && m_textEditor->LoadFile( prefsPath ) )
		{
			AppDebugOut( "Opened preferences file: %s\n", prefsPath );
		}
	}
}


void PreferencesEdit::Close()
{
	if ( m_textEditor )
	{
		m_textEditor->Close();
	}
}


void PreferencesEdit::Toggle()
{
	if ( IsOpen() )
	{
		Close();
	}
	else
	{
		Open();
	}
}
