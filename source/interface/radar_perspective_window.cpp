#include "lib/universal_include.h"

#include "lib/language_table.h"
#include "lib/gucci/window_manager.h"

#include "radar_perspective_window.h"
#include "lib/eclipse/components/core.h"

#include "app/app.h"
#include "app/globals.h"

#include "world/world.h"
#include "world/team.h"
#include "renderer/world_renderer.h"


class RadarOverlayToggleButton : public InterfaceButton
{
    void Render( int realX, int realY, bool highlighted, bool clicked )
    {
        bool on = g_app->GetWorldRenderer()->m_showRadar;
        SetCaption( on ? "dialog_radar_overlay_on" : "dialog_radar_overlay_off", true );
        
        InterfaceButton::Render( realX, realY, highlighted, clicked );
    }

    void MouseUp()
    {
        g_app->GetWorldRenderer()->m_showRadar = !g_app->GetWorldRenderer()->m_showRadar;
    }
};


class RadarPerspectiveTeamButton : public EclButton
{
public:
    int m_teamIndex;

    void Render( int realX, int realY, bool highlighted, bool clicked )
    {
        RadarPerspectiveWindow *parent = (RadarPerspectiveWindow *)m_parent;
        int teamId = parent->m_teamOrder[m_teamIndex];
        int myTeamId = g_app->GetWorld()->m_myTeamId;

        if( teamId == -1 )
        {
            Colour grey( 100, 100, 110, 178 );
            if( highlighted || clicked ) 
            {
                grey.m_a = 255;
            }

            Colour greyDark( 50, 50, 55, 255 );
            g_renderer2d->RectFill( realX, realY, m_w, m_h, grey, greyDark, greyDark, grey );
            g_renderer2d->Rect( realX, realY, m_w, m_h, Colour(255,255,255,100) );
            if( myTeamId == -1 )
            {
                g_renderer2d->Rect( realX, realY, m_w, m_h, Colour(255,255,255,255) );
            }
            if( teamId == parent->m_selectionTeamId )
            {
                g_renderer2d->Rect( realX-2, realY-2, m_w+4, m_h+4, Colour(255,255,255,255) );
            }

            g_renderer2d->TextSimple( realX+10, realY+6, White, 20, LANGUAGEPHRASE("dialog_c2s_default_name_spectator") );
            
            return;
        }

        Team *team = g_app->GetWorld()->GetTeam(teamId);
        if( team )
        {
            Colour teamCol = team->GetTeamColour();
            teamCol.m_a = 178;
            if( highlighted || clicked ) 
            {
                teamCol.m_a = 255;
            }

            Colour teamColDark = teamCol;
            teamColDark.m_r *= 0.2f;
            teamColDark.m_g *= 0.2f;
            teamColDark.m_b *= 0.2f;

            g_renderer2d->RectFill( realX, realY, m_w, m_h, teamCol, teamColDark, teamColDark, teamCol );
            g_renderer2d->Rect( realX, realY, m_w, m_h, Colour(255,255,255,100) );

            if( teamId == myTeamId )
            {
                g_renderer2d->Rect( realX, realY, m_w, m_h, Colour(255,255,255,255) );
            }
            if( teamId == parent->m_selectionTeamId )
            {
                g_renderer2d->Rect( realX-2, realY-2, m_w+4, m_h+4, Colour(255,255,255,255) );
            }

            g_renderer2d->TextSimple( realX+10, realY+6, White, 20, team->m_name );
        }
    }

    void MouseUp()
    {
        RadarPerspectiveWindow *parent = (RadarPerspectiveWindow *)m_parent;
        int teamId = parent->m_teamOrder[m_teamIndex];
        parent->m_selectionTeamId = teamId;

        extern int g_desiredPerspectiveTeamId;
        g_desiredPerspectiveTeamId = teamId;
        g_app->GetWorld()->m_myTeamId = teamId;
    }
};


RadarPerspectiveWindow::RadarPerspectiveWindow()
:   InterfaceWindow( "Radar Perspective", "dialog_radar_perspective", true ),
    m_selectionTeamId(-1)
{
    int numTeams = g_app->GetWorld()->m_teams.Size();
    int numRows = 1 + numTeams;

    SetSize( 400, 140 + numRows * 37 );
    SetPosition( g_windowManager->WindowW()/2 - m_w/2,
                 g_windowManager->WindowH()/2 - m_h/2 );

    for( int i = 0; i < MAX_TEAMS; ++i )
    {
        m_teamOrder[i] = -1;
    }

    m_teamOrder[0] = -1;

    m_selectionTeamId = g_app->GetWorld()->m_myTeamId;
    if( g_app->GetServer() && g_app->GetServer()->IsRecordingPlaybackMode() )
    {
        extern int g_desiredPerspectiveTeamId;
        if( g_desiredPerspectiveTeamId != -1 )
        {
            m_selectionTeamId = g_desiredPerspectiveTeamId;
        }
    }
}


void RadarPerspectiveWindow::Create()
{
    InterfaceWindow::Create();

    float xPos = 20;
    float yPos = 65;
    float width = m_w - 40;
    float height = 30;
    float gap = 7;
    int numTeams = g_app->GetWorld()->m_teams.Size();
    int numRows = 1 + numTeams;

    const int cornerMargin = 10;
    RadarOverlayToggleButton *radarToggle = new RadarOverlayToggleButton();
    radarToggle->SetProperties( "RadarOverlay", m_w - cornerMargin - 100, 30, 100, 18, "dialog_radar_overlay_off", " ", true, false );
    RegisterButton( radarToggle );

    InvertedBox *box = new InvertedBox();
    box->SetProperties( "box", xPos-10, yPos-10, width+20, (height+gap) * numRows + 15, " ", " ", false, false );
    RegisterButton( box );

    RadarPerspectiveTeamButton *spectator = new RadarPerspectiveTeamButton();
    spectator->SetProperties( "Spectator", xPos, yPos, width, height, " ", " ", false, false );
    spectator->m_teamIndex = 0;
    RegisterButton( spectator );
    yPos += height + gap;

    for( int i = 0; i < numTeams; ++i )
    {
        char name[256];
        sprintf( name, "Team %d", i );

        RadarPerspectiveTeamButton *team = new RadarPerspectiveTeamButton();
        team->SetProperties( name, xPos, yPos, width, height, " ", " ", false, false );
        team->m_teamIndex = i + 1;
        RegisterButton( team );

        yPos += height + gap;
    }

    CloseButton *close = new CloseButton();
    close->SetProperties( "Close", m_w - cornerMargin - 100, m_h - cornerMargin - 18, 100, 18, "dialog_close", " ", true, false );
    RegisterButton( close );
}


void RadarPerspectiveWindow::Update()
{
    m_teamOrder[0] = -1;

    LList<Team *> teams;
    for( int i = 0; i < g_app->GetWorld()->m_teams.Size(); ++i )
    {
        Team *team = g_app->GetWorld()->m_teams[i];
        teams.PutData( team );
    }

    int currentIndex = 1;
    while( teams.Size() > 0 )
    {
        Team *baseTeam = teams[0];
        m_teamOrder[currentIndex] = baseTeam->m_teamId;
        ++currentIndex;
        teams.RemoveData(0);

        for( int i = 0; i < teams.Size(); ++i )
        {
            Team *possibleAlly = teams[i];
            if( possibleAlly->m_allianceId == baseTeam->m_allianceId )
            {
                m_teamOrder[currentIndex] = possibleAlly->m_teamId;
                ++currentIndex;
                teams.RemoveData(i);
                --i;
            }
        }
    }

    int numRows = 1 + g_app->GetWorld()->m_teams.Size();
    m_h = 120 + numRows * 37;
    m_h = max( m_h, 280 );

    const int cornerMargin = 10;
    EclButton *closeBtn = GetButton( "Close" );
    if( closeBtn )
    {
        closeBtn->m_y = m_h - cornerMargin - 18;
    }
}


void RadarPerspectiveWindow::Render( bool _hasFocus )
{
    InterfaceWindow::Render( _hasFocus );

    g_renderer->SetFont( "kremlin" );

    int myTeamId = g_app->GetWorld()->m_myTeamId;
    char buf[256];
    strcpy( buf, LANGUAGEPHRASE("dialog_replay_viewing_as") );
    if( myTeamId == -1 )
    {
        LPREPLACESTRINGFLAG( 's', LANGUAGEPHRASE("dialog_c2s_default_name_spectator"), buf );
    }
    else
    {
        Team *viewingTeam = g_app->GetWorld()->GetTeam( myTeamId );
        if( viewingTeam )
        {
            LPREPLACESTRINGFLAG( 's', viewingTeam->m_name, buf );
        }
    }

    g_renderer2d->TextSimple( m_x + 10, m_y + 30, Colour(200,220,255), 20, buf );

    g_renderer->SetFont();
}

