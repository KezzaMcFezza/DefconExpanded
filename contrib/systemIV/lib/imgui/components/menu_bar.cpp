#include "systemiv.h"

#include "lib/debug/debug_utils.h"
#include "lib/preferences.h"

#include "imgui-1.92.5/imgui.h"
#include "menu_bar.h"
#include "lib/imgui/debug_console.h"
#include "lib/imgui/frame_debugger.h"
#include "lib/imgui/preferences_editor.h"

MenuBar::MenuBar()
	: m_isOpen( false ),
	  m_preferencesEdit( NULL ),
	  m_frameDebugger( false ),
	  m_windowReinitCallback( NULL )
{
	SetName( "MenuBar" );
	m_preferencesEdit = new PreferencesEdit();
	ImGuiRegisterWindow( m_preferencesEdit );
}


MenuBar::~MenuBar()
{
	if ( m_preferencesEdit )
	{
		ImGuiRemoveWindow( "Preferences Editor" );
		delete m_preferencesEdit;
		m_preferencesEdit = NULL;
	}
}


void MenuBar::ReregisterWindows()
{
	if ( m_preferencesEdit )
	{
		ImGuiRegisterWindow( m_preferencesEdit );
	}
}


void MenuBar::Render()
{
	if ( !m_isOpen )
		return;

	if ( ImGui::BeginMainMenuBar() )
	{
		if ( ImGui::BeginMenu( "File" ) )
		{
			if ( ImGui::MenuItem( "Save Preferences" ) )
			{
				if ( g_preferences )
				{
					g_preferences->Save();
					AppDebugOut( "Preferences saved\n" );
				}
			}

			ImGui::EndMenu();
		}

		if ( ImGui::BeginMenu( "Edit" ) )
		{
			if ( ImGui::MenuItem( "Edit Preferences" ) )
			{
				if ( m_preferencesEdit )
				{
					m_preferencesEdit->Open();
				}
			}

			ImGui::EndMenu();
		}

		if ( ImGui::BeginMenu( "Debug" ) )
		{
			if ( ImGui::MenuItem( "Console", "~" ) )
			{
				DebugConsole *console = DebugConsole::GetInstance();
				if ( console )
				{
					console->Toggle();
				}
			}

			ImGui::EndMenu();
		}

		if ( ImGui::BeginMenu( "Window" ) )
		{
			if ( ImGui::MenuItem( "Reinitialize Window" ) )
			{
				if ( m_windowReinitCallback )
				{
					m_windowReinitCallback();
					AppDebugOut( "Window reinitialization requested\n" );
				}
			}

			if ( ImGui::MenuItem( "Relaunch Game" ) )
			{
				RelaunchApplication();
			}

			ImGui::EndMenu();
		}

		if ( ImGui::BeginMenu( "Rendering" ) )
		{
			if ( ImGui::MenuItem( "Frame Debugger", "F2" ) )
			{
				FrameDebugger *frameDebugger = FrameDebugger::GetInstance();
				if ( frameDebugger )
				{
					frameDebugger->Toggle();
				}
			}

			ImGui::EndMenu();
		}

		ImGui::EndMainMenuBar();
	}
}
