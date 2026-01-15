#include "systemiv.h"

#include "lib/debug/debug_utils.h"
#include "lib/preferences.h"

#include "imgui-1.92.5/imgui.h"
#include "menu_bar.h"
#include "lib/imgui/debug_console.h"
#include "lib/imgui/preferences_editor.h"
#include "lib/render2d/renderer_2d.h"
#include "lib/render3d/renderer_3d.h"

MenuBar::MenuBar()
	: m_isOpen( false ),
	  m_preferencesEdit( NULL ),
	  m_immediateMode2D( false ),
	  m_immediateMode3D( false ),
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


void MenuBar::Draw()
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

#ifndef TARGET_EMSCRIPTEN
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
#endif

		if ( ImGui::BeginMenu( "Rendering" ) )
		{
			if ( ImGui::MenuItem( "Immediate Mode 2D", NULL, &m_immediateMode2D ) )
			{
				AppDebugOut( "Immediate Mode 2D: %s\n", m_immediateMode2D ? "enabled" : "disabled" );

				if ( g_renderer2d )
				{
					g_renderer2d->SetImmediateModeEnabled( m_immediateMode2D );
				}
			}

			if ( ImGui::MenuItem( "Immediate Mode 3D", NULL, &m_immediateMode3D ) )
			{
				AppDebugOut( "Immediate Mode 3D: %s\n", m_immediateMode3D ? "enabled" : "disabled" );

				if ( g_renderer3d )
				{
					g_renderer3d->SetImmediateModeEnabled3D( m_immediateMode3D );
				}
			}

			ImGui::Separator();

			if ( ImGui::MenuItem( "Frame Debugger", NULL, &m_frameDebugger ) )
			{
				AppDebugOut( "Frame Debugger: %s\n", m_frameDebugger ? "enabled" : "disabled" );
			}

			ImGui::EndMenu();
		}

		ImGui::EndMainMenuBar();
	}
}
