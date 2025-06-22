#include "lib/universal_include.h"

#include <stdio.h>
#include <string.h>

#include "lib/gucci/input.h"
#include "lib/render/renderer.h"
#include "lib/profiler.h"
#include "lib/language_table.h"

#include "profile_window.h"



// ****************************************************************************
// Class ProfilerButton
// ****************************************************************************

class ProfilerButton : public InterfaceButton
{
public:
    void MouseUp()
    {
		if (stricmp(m_caption, "dialog_profile_toggle_glfinish") == 0)
		{
			g_profiler->m_doGlFinish = !g_profiler->m_doGlFinish;
		}
		else if (stricmp(m_caption, "dialog_profile_reset_history") == 0)
		{
			g_profiler->ResetHistory();
		}
		else if (stricmp(m_caption, "dialog_profile_min") == 0)
		{
			strcpy(m_caption, "dialog_profile_avg");
		}
		else if (stricmp(m_caption, "dialog_profile_avg") == 0)
		{
			strcpy(m_caption, "dialog_profile_max");
		}
		else if (stricmp(m_caption, "dialog_profile_max") == 0)
		{
			strcpy(m_caption, "dialog_profile_min");
		}
    }
};



// ****************************************************************************
// Class ProfileWindow
// ****************************************************************************

static int s_refCount = 0;

ProfileWindow::ProfileWindow( char *name, char *title, bool titleIsLanguagePhrase )
:   InterfaceWindow( name, title, titleIsLanguagePhrase )
{
    SetSize( 500, 500 );

    if( s_refCount == 0 )
    {
        Profiler::Start();
    }

    ++s_refCount;
}


ProfileWindow::~ProfileWindow()
{
	--s_refCount;

    if( s_refCount == 0 )
    {
        Profiler::Stop();
    }
}


void ProfileWindow::RenderElementProfile(ProfiledElement *_pe, unsigned int _indent)
{
	if (_pe->m_children.NumUsed() == 0) return;

    int left = m_x + 10;
    char caption[256];
	EclButton *minAvgMaxButton = GetButton("Avg");
	int minAvgMax = 0;
	if (stricmp(minAvgMaxButton->m_caption, "dialog_profile_avg") == 0)
	{
		minAvgMax = 1;
	}
	else if (stricmp(minAvgMaxButton->m_caption, "dialog_profile_max") == 0)
	{
		minAvgMax = 2;
	}


	float largestTime = 1000.0f * _pe->GetMaxChildTime();
	float totalTime = 0.0f;

	short i = _pe->m_children.StartOrderedWalk();
	while (i != -1)
    {
        ProfiledElement *child = _pe->m_children[i];

        float time = float( child->m_lastTotalTime * 1000.0f );
		float avrgTime = 1000.0f * child->m_historyTotalTime / child->m_historyNumCalls;
		if (avrgTime > 0.0f)
		{
			totalTime += time;

			char icon[] = " ";
			if (child->m_children.NumUsed() > 0) 
			{
				icon[0] = child->m_isExpanded ? '-' : '+';
			}

			float lastColumn;
			if (minAvgMax == 0)			lastColumn = child->m_shortest;
			else if (minAvgMax == 1)	lastColumn = avrgTime / 1000.0f;
			else if (minAvgMax == 2)	lastColumn = child->m_longest;
			lastColumn *= 1000.0f;

            if( child->m_lastNumCalls > 0 )
            {
			    sprintf(caption, 
                        "%*s%-*s:%5d x%5.2f = %4.0f (%f)",
					    _indent + 1,
					    icon,
					    24 - _indent,
					    child->m_name, 
					    child->m_lastNumCalls, 
					    time/(float)child->m_lastNumCalls,
					    time,
					    lastColumn);
			    int brightness = (time / largestTime) * 150.0f + 105.0f;
			    if (brightness < 105) brightness = 105;
			    else if (brightness > 255) brightness = 255;
			    // Color now passed directly to TextSimple() - no need for glColor3ub

			    // Deal with mouse clicks to expand or unexpand a node
			    if (g_inputManager->GetRawLmbClicked())
			    {
				    int x = g_inputManager->m_mouseX;
				    int y = g_inputManager->m_mouseY;
				    if (x > m_x && x < m_x+m_w &&
					    y > m_yPos+12 && y < m_yPos + 24)
				    {
					    AppReleaseAssert(child != g_profiler->m_rootElement,
						    "ProfileWindow::RenderElementProfile child==root");
					    child->m_isExpanded = !child->m_isExpanded;
				    }
			    }
                
                g_renderer->TextSimple( left, m_yPos+=12, Colour(brightness,brightness,brightness), 12, caption );

                int lineLeft = left + 310;
                int lineY = m_yPos;
                
                int availableWidth = m_x+m_w - lineLeft - 30;
                int lineWidth = availableWidth * sqrt(time)/sqrtf(1000.0f);
                
                //int lineWidth = (sqrtf(time)/sqrtf(largestTime)) * availableWidth;
                int lineHeight = 11.0f;

                // Color now passed directly to Rect/RectFill() - no need for glColor4ub

                if(child->m_isExpanded)
                {
                    // Convert GL_LINES to modern Rect() function for outlined rectangle
                    Colour lineColor(150, 150, 250, brightness);
                    g_renderer->Rect( lineLeft, lineY, lineWidth, lineHeight, lineColor );
                }
                else            
                {
                    // Convert GL_QUADS to modern RectFill() function for filled rectangle
                    Colour fillColor(150, 150, 250, brightness);
                    g_renderer->RectFill( lineLeft, lineY, lineWidth, lineHeight, fillColor );
                }
                
			    if (child->m_isExpanded && child->m_children.NumUsed() > 0)
			    {
				    RenderElementProfile(child, _indent + 2);
			    }
            }
		}

		i = _pe->m_children.GetNextOrderedIndex();
    }
    
    m_yPos+=6;
}


void ProfileWindow::RenderFrameTimes( float x, float y, float w, float h )
{
    if( g_profiler )
    {
		char caption[128];
        strcpy( caption, LANGUAGEPHRASE("dialog_mapr_fps") );
		LPREPLACEINTEGERFLAG( 'F', 5, caption );
        g_renderer->Line( x, y, x + w, y, Colour(255,255,255,100), 1.0f );
        g_renderer->Text( x, y-10, White, 10, caption );

        strcpy( caption, LANGUAGEPHRASE("dialog_mapr_fps") );
		LPREPLACEINTEGERFLAG( 'F', 10, caption );
        g_renderer->Line( x, y + h/2, x + w, y + h/2, Colour(255,255,255,100), 1.0f );
        g_renderer->Text( x, y + h/2 -10, White, 10, caption );

        strcpy( caption, LANGUAGEPHRASE("dialog_mapr_fps") );
		LPREPLACEINTEGERFLAG( 'F', 20, caption );
        g_renderer->Line( x, y + h * 3/4, x + w, y + h * 3/4, Colour(255,255,255,100), 1.0f );
        g_renderer->Text( x, y + h * 3/4 -10, White, 10, caption );

        // Color now passed directly to Line() - modern OpenGL renderer

        float xPos = x + w;
        float wPerFrame = w / 200;

        // Convert GL_LINES immediate mode to modern Line() function calls
        Colour frameLineColor(255, 0, 0, (int)(0.8f * 255)); // Modern OpenGL color format

        for( int i = 0; i < g_profiler->m_frameTimes.Size(); ++i )
        {
            int thisFrameTime = g_profiler->m_frameTimes[i];

            float frameHeight = 0.5f * (thisFrameTime / 100.0f) * h;

            // Draw each frame time as individual line using modern renderer
            g_renderer->Line( xPos, y + h, xPos, y + h - frameHeight, frameLineColor );

            xPos -= wPerFrame;
        }
    }
}


void ProfileWindow::Render( bool hasFocus )
{
    InterfaceWindow::Render( hasFocus );
    
    if (g_profiler->m_doGlFinish)
	{
        g_renderer->TextSimple( m_x+160, m_y+28, White, 12, LANGUAGEPHRASE("dialog_yes") );
	}
	else
	{
        g_renderer->TextSimple( m_x+160, m_y+28, White, 12, LANGUAGEPHRASE("dialog_no") );
	}

    ProfiledElement *root = g_profiler->m_rootElement;
    int tableSize = root->m_children.Size();

    m_yPos = m_y + 42;

    g_renderer->SetFont( "zerothre", false, false, true );

	RenderElementProfile(root, 0);

    g_renderer->SetFont();

    RenderFrameTimes( m_x + 10, m_y + m_h - 100, m_w - 20, 90 );

    m_h = m_yPos - m_y + 150;
    GetButton("Invert")->m_h = m_h - 60;
}


void ProfileWindow::Create()
{
	InterfaceWindow::Create();

    InvertedBox *invert = new InvertedBox();
    invert->SetProperties( "Invert", 10, 50, m_w-20, m_h-60, " ", " ", false, false );
    RegisterButton( invert );
    
	ProfilerButton *but = new ProfilerButton();
	but->SetProperties("Toggle glFinish", 10, 25, 130, 18, "dialog_profile_toggle_glfinish", " ", true, false );
	RegisterButton(but);

	ProfilerButton *resetHistBut = new ProfilerButton();
	resetHistBut->SetProperties("Reset History", m_w - 10 - 130 - 10 - 130, 25, 130, 18, "dialog_profile_reset_history", " ", true, false );
	RegisterButton(resetHistBut);

	ProfilerButton *minAvgMax = new ProfilerButton();
	minAvgMax->SetProperties("Avg", m_w - 130 - 10, 25, 130, 18, "dialog_profile_avg", " ", true, false );
	RegisterButton(minAvgMax);

	g_profiler->m_doGlFinish = true;
}
