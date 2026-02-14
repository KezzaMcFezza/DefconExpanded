#include "systemiv.h"

#include "lib/debug/debug_utils.h"
#include "preferences_editor.h"
#include "components/text_editor.h"

PreferencesEdit::PreferencesEdit()
	: m_textEditor( NULL )
{
	SetName( "Preferences Editor" );
}


PreferencesEdit::~PreferencesEdit()
{
	Shutdown();
}


void PreferencesEdit::Init()
{
	if ( !m_textEditor )
	{
		m_textEditor = new TextEditor();
	}
}


void PreferencesEdit::Shutdown()
{
	if ( m_textEditor )
	{
		delete m_textEditor;
		m_textEditor = NULL;
	}
}


void PreferencesEdit::Update()
{
}


void PreferencesEdit::Window()
{
	ImGuiSetWindowLayout(
		PREFERENCES_EDITOR_DEFAULT_X_PERCENT,
		PREFERENCES_EDITOR_DEFAULT_Y_PERCENT,
		PREFERENCES_EDITOR_DEFAULT_WIDTH_PERCENT,
		PREFERENCES_EDITOR_DEFAULT_HEIGHT_PERCENT );
}


void PreferencesEdit::Render()
{
	if ( !IsOpen() )
		return;

	if ( !m_textEditor )
		return;

	Window();
	m_textEditor->Render();
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
