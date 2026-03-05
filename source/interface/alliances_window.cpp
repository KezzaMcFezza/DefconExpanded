#include "lib/universal_include.h"

#include "lib/language_table.h"
#include "lib/gucci/window_manager.h"
#include "lib/resource/resource.h"

#include "alliances_window.h"

#include "app/app.h"
#include "app/globals.h"
#include "app/game.h"
#include "lib/multiline_text.h"

#include "world/world.h"
#include "world/team.h"

#include "renderer/map_renderer.h"


// Close button that also removes the colour-pick window when Alliances is closed
class CloseAlliancesButton : public CloseButton
{
public:
    void MouseUp()
    {
        if( EclGetWindow( "Alliances" ) )
            EclRemoveWindow( "Alliances" );
        if( EclGetWindow( "ALLIANCE COLOR" ) )
            EclRemoveWindow( "ALLIANCE COLOR" );
    }
};


class JoinLeaveButton : public InterfaceButton
{    
    void Render( int realX, int realY, bool highlighted, bool clicked )
    {
        if( g_app->GetGame()->m_winner != -1 ) return;
        if( g_app->GetWorld()->AmISpectating() ) return;

        int permitDefection = g_app->GetGame()->GetOptionValue( "PermitDefection" );
        if( permitDefection )
        {
            AlliancesWindow *parent = (AlliancesWindow *)m_parent;
            int teamId = parent->m_selectionTeamId;

            if( teamId == -1 ) return;
            
            Team *myTeam = g_app->GetWorld()->GetMyTeam();
            Team *thisTeam = g_app->GetWorld()->GetTeam(teamId);
        
            if( myTeam && thisTeam )
            {
                if( myTeam->m_allianceId == thisTeam->m_allianceId )
                {
                    if( myTeam == thisTeam )
                    {
                        // This is my team
                        if( g_app->GetWorld()->CountAllianceMembers(myTeam->m_allianceId) == 1 )
                        {
                            // I am the only member of this alliance, so theres nothing to do here
                            return;
                        }
                        SetCaption( "dialog_alliance_leave", true );
                        SetTooltip( "tooltip_alliance_leave", true );
                    }
                    else
                    {
                        // Another member of the same alliance
                        SetCaption( "dialog_alliance_kick", true );
                        SetTooltip( "tooltip_alliance_kick", true );
                    }
                }
                else if( myTeam->m_allianceId != thisTeam->m_allianceId )
                {
                    // A different (enemy) alliance
                    SetCaption( "dialog_alliance_join", true );
                    SetTooltip( "tooltip_alliance_join", true );
                }
            }
        
            InterfaceButton::Render( realX, realY, highlighted, clicked );
        }
    }

    void MouseUp()
    {
        if( g_app->GetGame()->m_winner != -1 ) return;
        if( g_app->GetWorld()->AmISpectating() ) return;

        int permitDefection = g_app->GetGame()->GetOptionValue( "PermitDefection" );
        if( permitDefection )
        {
            AlliancesWindow *parent = (AlliancesWindow *)m_parent;
            int teamId = parent->m_selectionTeamId;

            Team *myTeam = g_app->GetWorld()->GetMyTeam();
            Team *thisTeam = g_app->GetWorld()->GetTeam(teamId);
        
            if( myTeam && thisTeam )
            {
                if( myTeam->m_allianceId == thisTeam->m_allianceId )
                {
                    if( myTeam == thisTeam )
                    {
                        // This is my team
                        if( g_app->GetWorld()->CountAllianceMembers(myTeam->m_allianceId) > 1 )
                        {
                            g_app->GetClientToServer()->BeginVote( g_app->GetWorld()->m_myTeamId,
                                                                   Vote::VoteTypeLeaveAlliance,
                                                                   g_app->GetWorld()->m_myTeamId );
                        }
                    }
                    else
                    {
                        // Another member of the same alliance
                        g_app->GetClientToServer()->BeginVote( g_app->GetWorld()->m_myTeamId,
                                                               Vote::VoteTypeKickPlayer,
                                                               teamId );
                    }
                }
                else if( myTeam->m_allianceId != thisTeam->m_allianceId )
                {
                    // JOIN A different (enemy) alliance
                    g_app->GetClientToServer()->BeginVote( g_app->GetWorld()->m_myTeamId, 
                                                           Vote::VoteTypeJoinAlliance, 
                                                           thisTeam->m_allianceId );
                }
            }
        }
    }
};


class ConfirmShareRadarButton : public InterfaceButton
{
public:
    void MouseUp()
    {
        ShareRadarWindow *parent = (ShareRadarWindow *)m_parent;
        int teamId = parent->m_teamId;
        Team *myTeam = g_app->GetWorld()->GetMyTeam();
        Team *thisTeam = g_app->GetWorld()->GetTeam(teamId);

        if( myTeam &&
            thisTeam &&
            myTeam != thisTeam )
        {
            g_app->GetClientToServer()->RequestShareRadar( myTeam->m_teamId, teamId );
            EclRemoveWindow( m_parent->m_name );
        }
    }
};

ShareRadarWindow::ShareRadarWindow( int teamId )
:   InterfaceWindow( "Share Radar", "dialog_shareradar", true )
{
    m_teamId = teamId;
    SetSize( 280, 160 );
    Centralise();
}

void ShareRadarWindow::Create()
{
    ConfirmShareRadarButton *yes = new ConfirmShareRadarButton();
    yes->SetProperties( "ShareRadarWindowYes", 20, m_h-30, 80, 20, "dialog_yes", "", true, false );
    RegisterButton( yes );

    CloseButton *no = new CloseButton();
    no->SetProperties( "ShareRadarWindowNo", m_w-100, m_h-30, 80, 20, "dialog_no", "", true, false );
    RegisterButton( no );
}

void ShareRadarWindow::Render( bool hasFocus )
{
    InterfaceWindow::Render( hasFocus );
    char msg[1024];
    const char *helpMessage = NULL;

    if( g_app->GetWorld()->GetMyTeam()->m_sharingRadar[m_teamId] )
    {
        strcpy( msg, LANGUAGEPHRASE("dialog_shareradarend"));
        LPREPLACESTRINGFLAG('T', g_app->GetWorld()->GetTeam( m_teamId )->GetTeamName(), msg );
        helpMessage = LANGUAGEPHRASE("tooltip_radarshare_disable" );
    }
    else
    {
        strcpy( msg, LANGUAGEPHRASE("dialog_shareradarbegin"));
        LPREPLACESTRINGFLAG('T', g_app->GetWorld()->GetTeam( m_teamId )->GetTeamName(), msg );
        helpMessage = LANGUAGEPHRASE("tooltip_radarshare_enable" );
    }
    g_renderer2d->TextCentreSimple( m_x+m_w/2, m_y+25, White, 15, msg );


    //
    // Render tooltip

    float xPos = m_x + 30;
    float yPos = m_y + 40;
    float w = m_w - 60;

    MultiLineText wrapped( helpMessage, w, 11 );

    for( int i = 0; i < wrapped.Size(); ++i )
    {
        char *thisString = wrapped[i];
        g_renderer2d->TextSimple( xPos, yPos+=13, White, 11, thisString );
    }
}


// ============================================================================

class ConfirmCeaseFireButton : public InterfaceButton
{
public:
    void MouseUp()
    {
        CeaseFireWindow *parent = (CeaseFireWindow *)m_parent;
        int teamId = parent->m_teamId;
        Team *myTeam = g_app->GetWorld()->GetMyTeam();
        Team *thisTeam = g_app->GetWorld()->GetTeam(teamId);

        if( myTeam &&
            thisTeam &&
            myTeam != thisTeam )
        {
            g_app->GetClientToServer()->RequestCeaseFire( myTeam->m_teamId, teamId );
            EclRemoveWindow( m_parent->m_name );
        }
    }
};

CeaseFireWindow::CeaseFireWindow( int teamId )
:   InterfaceWindow( "Cease Fire", "dialog_ceasefire_title", true )
{
    m_teamId = teamId;
    SetSize( 280, 150 );
    Centralise();
}

void CeaseFireWindow::Create()
{
    ConfirmCeaseFireButton *yes = new ConfirmCeaseFireButton();
    yes->SetProperties( "CeaseFireYes", 20, m_h-30, 80, 20, "dialog_yes", "", true, false );
    RegisterButton( yes );

    CloseButton *no = new CloseButton();
    no->SetProperties( "CeaseFireNo", m_w-100, m_h-30, 80, 20, "dialog_no", "", true, false );
    RegisterButton( no );
}

void CeaseFireWindow::Render( bool hasFocus )
{
    InterfaceWindow::Render( hasFocus );
    char msg[1024];
    const char *helpMessage = NULL;

    if( g_app->GetWorld()->GetMyTeam()->m_ceaseFire[m_teamId] )
    {
        strcpy( msg, LANGUAGEPHRASE("dialog_declarewar"));
        LPREPLACESTRINGFLAG('T', g_app->GetWorld()->GetTeam( m_teamId )->GetTeamName(), msg );
        helpMessage = LANGUAGEPHRASE("tooltip_ceasefire_disable" );
    }
    else
    {
        strcpy( msg, LANGUAGEPHRASE("dialog_ceasefire"));
        LPREPLACESTRINGFLAG('T', g_app->GetWorld()->GetTeam( m_teamId )->GetTeamName(), msg );
        helpMessage = LANGUAGEPHRASE("tooltip_ceasefire_enable" );
    }
    g_renderer2d->TextCentreSimple( m_x+m_w/2, m_y+25, White, 17, msg );


    //
    // Render tooltip

    float xPos = m_x + 30;
    float yPos = m_y + 40;
    float w = m_w - 60;

    MultiLineText wrapped( helpMessage, w, 11 );

    for( int i = 0; i < wrapped.Size(); ++i )
    {
        char *thisString = wrapped[i];
        g_renderer2d->TextSimple( xPos, yPos+=13, White, 11, thisString );
    }
}


// ============================================================================


class VoteButton : public EclButton
{
public:
    int m_voteId;
    int m_vote;

    void Render( int realX, int realY, bool highlighted, bool clicked )
    {
        Image *img = g_resource->GetImage( "gui/tick.bmp" );
        
        Vote *vote = g_app->GetWorld()->m_votingSystem.LookupVote(m_voteId);
        if( vote && vote->m_result == Vote::VoteUnknown )
        {   
            Team *myTeam = g_app->GetWorld()->GetMyTeam();
            if( vote->GetRequiredAllianceId() == myTeam->m_allianceId )
            {       
                g_renderer->SetBlendMode( Renderer::BlendModeAdditive );

                g_renderer2d->RectFill ( realX, realY, m_w, m_h, Colour(100,100,100,50), true );

                if( highlighted || clicked )
                {
                    g_renderer2d->RectFill( realX, realY, m_w, m_h, Colour(100,100,100,150), true );
                    g_renderer2d->StaticSprite( img, realX+8, realY+2, 15, 15, Colour(255,255,255,20), true );
                }

                if( vote->GetCurrentVote( myTeam->m_teamId ) == m_vote )
                {
                    g_renderer2d->RectFill( realX, realY, m_w, m_h, Colour(100,100,100,150), true );
                    g_renderer2d->StaticSprite( img, realX+8, realY+2, 15, 15, White, true );
                }

                g_renderer2d->Rect( realX, realY, m_w, m_h, Colour(100,100,100,100) );

                g_renderer->SetBlendMode( Renderer::BlendModeNormal );
            }
        }
    }

    void MouseUp()
    {
        Vote *vote = g_app->GetWorld()->m_votingSystem.LookupVote(m_voteId);
        if( vote && vote->m_result == Vote::VoteUnknown )
        {   
            Team *myTeam = g_app->GetWorld()->GetMyTeam();
            if( vote->GetRequiredAllianceId() == myTeam->m_allianceId )
            {
                g_app->GetClientToServer()->CastVote( myTeam->m_teamId, m_voteId, m_vote );
            }
        }
    }
};


VotingWindow::VotingWindow()
:   InterfaceWindow("Vote", "dialog_vote", true)
{
    SetSize( 300, 270 );
    Centralise();
}


void VotingWindow::Create()
{
    InterfaceWindow::Create();

    InvertedBox *box = new InvertedBox();
    box->SetProperties( "invert", 20, 80, m_w-40, 100, " ", " ", false, false );
    RegisterButton( box );

    VoteButton *voteYes = new VoteButton();
    voteYes->SetProperties( "yes", 40, 90, 30, 20, " ", "tooltip_vote_yes", false, true );
    voteYes->m_vote = Vote::VoteYes;
    voteYes->m_voteId = m_voteId;
    RegisterButton( voteYes );

    VoteButton *voteNo = new VoteButton();
    voteNo->SetProperties( "no", 40, 120, 30, 20, " ", "tooltip_vote_no", false, true );
    voteNo->m_vote = Vote::VoteNo;
    voteNo->m_voteId = m_voteId;
    RegisterButton( voteNo );

    VoteButton *voteAbstain = new VoteButton();
    voteAbstain->SetProperties( "abstain", 40, 150, 30, 20, " ", "tooltip_vote_abstain", false, true );
    voteAbstain->m_vote = Vote::VoteAbstain;
    voteAbstain->m_voteId = m_voteId;
    RegisterButton( voteAbstain );

    CloseButton *close = new CloseButton();
    close->SetProperties( "Close", m_w/2 - 50, m_h - 25, 100, 20, "dialog_close", " ", true, false );
    RegisterButton( close );
}


void VotingWindow::Render( bool _hasFocus )
{
    InterfaceWindow::Render( _hasFocus );

    Vote *vote = g_app->GetWorld()->m_votingSystem.LookupVote(m_voteId);
    if( vote )
    {   
        Team *myTeam = g_app->GetWorld()->GetMyTeam();

        switch( vote->m_voteType )
        {
            case Vote::VoteTypeJoinAlliance:
                if( myTeam->m_teamId == vote->m_createTeamId )
                {
                    const char *allianceName = g_app->GetWorld()->GetAllianceName(vote->m_voteData);
                    g_renderer2d->TextCentreSimple( m_x+m_w/2, m_y+30, White, 20, LANGUAGEPHRASE("dialog_you_requested_alliance_1") );
					char caption[512];
                    strcpy( caption, LANGUAGEPHRASE("dialog_you_requested_alliance_2") );
					LPREPLACESTRINGFLAG( 'A', allianceName, caption );
                    g_renderer2d->TextCentreSimple( m_x+m_w/2, m_y+50, White, 20, caption );
                }
                else if( myTeam->m_allianceId == vote->m_voteData )
                {
                    Team *team = g_app->GetWorld()->GetTeam( vote->m_createTeamId );
					char caption[512];
                    strcpy( caption, LANGUAGEPHRASE("dialog_requested_alliance_1") );
					LPREPLACESTRINGFLAG( 'T', team->m_name, caption );
					g_renderer2d->TextCentreSimple( m_x+m_w/2, m_y+30, White, 20, caption );
                    g_renderer2d->TextCentreSimple( m_x+m_w/2, m_y+50, White, 20, LANGUAGEPHRASE("dialog_requested_alliance_2") );
                }
                break;

            case Vote::VoteTypeLeaveAlliance:
                if( myTeam->m_teamId == vote->m_createTeamId )
                {
                    const char *p1 = g_languageTable->DoesPhraseExist("dialog_you_requested_leave_1") ? LANGUAGEPHRASE("dialog_you_requested_leave_1") : "You have requested to";
                    const char *p2 = g_languageTable->DoesPhraseExist("dialog_you_requested_leave_2") ? LANGUAGEPHRASE("dialog_you_requested_leave_2") : "leave your Alliance";
                    g_renderer2d->TextCentreSimple( m_x+m_w/2, m_y+30, White, 20, p1 );
                    g_renderer2d->TextCentreSimple( m_x+m_w/2, m_y+50, White, 20, p2 );
                }
                else if( myTeam->m_allianceId == g_app->GetWorld()->GetTeam( vote->m_createTeamId )->m_allianceId )
                {
                    Team *team = g_app->GetWorld()->GetTeam( vote->m_createTeamId );
                    char caption[512];
                    strcpy( caption, g_languageTable->DoesPhraseExist("dialog_requested_leave_1") ? LANGUAGEPHRASE("dialog_requested_leave_1") : "*T has requested to" );
                    LPREPLACESTRINGFLAG( 'T', team->m_name, caption );
                    g_renderer2d->TextCentreSimple( m_x+m_w/2, m_y+30, White, 20, caption );
                    const char *p2 = g_languageTable->DoesPhraseExist("dialog_requested_leave_2") ? LANGUAGEPHRASE("dialog_requested_leave_2") : "leave the Alliance";
                    g_renderer2d->TextCentreSimple( m_x+m_w/2, m_y+50, White, 20, p2 );
                }
                break;

            case Vote::VoteTypeMigrateAlliance:
                if( myTeam->m_teamId == vote->m_createTeamId )
                {
                    const char *allianceName = g_app->GetWorld()->GetAllianceName(vote->m_voteData);
                    const char *p1 = g_languageTable->DoesPhraseExist("dialog_you_req_migrate_alliance_1") ? LANGUAGEPHRASE("dialog_you_req_migrate_alliance_1") : "You requested to";
                    g_renderer2d->TextCentreSimple( m_x+m_w/2, m_y+30, White, 20, p1 );
                    char caption[512];
                    strcpy( caption, g_languageTable->DoesPhraseExist("dialog_you_req_migrate_alliance_2") ? LANGUAGEPHRASE("dialog_you_req_migrate_alliance_2") : "change your Alliance color to *A" );
                    LPREPLACESTRINGFLAG( 'A', allianceName, caption );
                    g_renderer2d->TextCentreSimple( m_x+m_w/2, m_y+50, White, 20, caption );
                }
                else if( myTeam->m_allianceId == g_app->GetWorld()->GetTeam( vote->m_createTeamId )->m_allianceId )
                {
                    const char *allianceName = g_app->GetWorld()->GetAllianceName(vote->m_voteData);
                    Team *team = g_app->GetWorld()->GetTeam( vote->m_createTeamId );
                    char caption[512];
                    strcpy( caption, g_languageTable->DoesPhraseExist("dialog_requested_migrate_alliance_1") ? LANGUAGEPHRASE("dialog_requested_migrate_alliance_1") : "*T requested to" );
                    LPREPLACESTRINGFLAG( 'T', team->m_name, caption );
                    g_renderer2d->TextCentreSimple( m_x+m_w/2, m_y+30, White, 20, caption );
                    strcpy( caption, g_languageTable->DoesPhraseExist("dialog_requested_migrate_alliance_2") ? LANGUAGEPHRASE("dialog_requested_migrate_alliance_2") : "change Alliance color to *A" );
                    LPREPLACESTRINGFLAG( 'A', allianceName, caption );
                    g_renderer2d->TextCentreSimple( m_x+m_w/2, m_y+50, White, 20, caption );
                }
                break;

            case Vote::VoteTypeKickPlayer:
            {
                Team *kickTeam = g_app->GetWorld()->GetTeam(vote->m_voteData);
                if( myTeam->m_teamId == vote->m_createTeamId )
                {
                    g_renderer2d->TextCentreSimple( m_x+m_w/2, m_y+30, White, 20, LANGUAGEPHRASE("dialog_you_requested_kick_1") );
					char caption[512];
                    strcpy( caption, LANGUAGEPHRASE("dialog_you_requested_kick_2") );
					LPREPLACESTRINGFLAG( 'T', kickTeam->m_name, caption );
                    g_renderer2d->TextCentreSimple( m_x+m_w/2, m_y+50, White, 20, caption );
                }
                else if( myTeam->m_allianceId == kickTeam->m_allianceId &&
                         myTeam->m_teamId != kickTeam->m_teamId )
                {
                    Team *team = g_app->GetWorld()->GetTeam( vote->m_createTeamId );
					char caption[512];
                    strcpy( caption, LANGUAGEPHRASE("dialog_requested_kick_1") );
					LPREPLACESTRINGFLAG( 'T', team->m_name, caption );
                    g_renderer2d->TextCentreSimple( m_x+m_w/2, m_y+30, White, 20, caption );
                    strcpy( caption, LANGUAGEPHRASE("dialog_requested_kick_2") );
					LPREPLACESTRINGFLAG( 'T', kickTeam->m_name, caption );
                    g_renderer2d->TextCentreSimple( m_x+m_w/2, m_y+50, White, 20, caption );
                }
                break;
            }
        }


        g_renderer->SetFont( "kremlin" );
        g_renderer2d->TextCentreSimple( m_x + m_w/2, m_y + 90, White, 20, LANGUAGEPHRASE("dialog_vote_yes") );
        g_renderer2d->TextCentreSimple( m_x + m_w/2, m_y + 120, White, 20, LANGUAGEPHRASE("dialog_vote_no") );
        g_renderer2d->TextCentreSimple( m_x + m_w/2, m_y + 150, White, 20, LANGUAGEPHRASE("dialog_vote_abstain") );
        g_renderer->SetFont();

        int yes, no, abstain;
        vote->GetCurrentVote( &yes, &no, &abstain );

        g_renderer2d->Text( m_x + m_w - 40, m_y + 90, White, 20, "%d", yes );
        g_renderer2d->Text( m_x + m_w - 40, m_y + 120, White, 20, "%d", no );
        g_renderer2d->Text( m_x + m_w - 40, m_y + 150, White, 20, "%d", abstain );

        if( vote->m_result == Vote::VoteUnknown )
        {
			char caption[512];
            strcpy( caption, LANGUAGEPHRASE("dialog_vote_seconds_to_vote") );
			LPREPLACEINTEGERFLAG( 'S', (int) vote->m_timer, caption );
            g_renderer2d->TextCentreSimple( m_x + m_w/2, m_y + m_h - 70, White, 15, caption );
            
            int votesRequired = vote->GetVotesRequired();
            strcpy( caption, LANGUAGEPHRASE("dialog_vote_number_votes_required") );
			LPREPLACEINTEGERFLAG( 'V', votesRequired, caption );
            g_renderer2d->TextCentreSimple( m_x+m_w/2, m_y + m_h - 50, White, 15, caption );
        }
        else if( vote->m_result == Vote::VoteYes )
        {
            g_renderer->SetFont( "kremlin" );
            g_renderer2d->TextCentreSimple( m_x+m_w/2, m_y + m_h - 70, White, 30, LANGUAGEPHRASE("dialog_vote_succeeded") );
            g_renderer->SetFont();
        }
        else if( vote->m_result == Vote::VoteNo )
        {
            g_renderer->SetFont( "kremlin" );
            g_renderer2d->TextCentreSimple( m_x+m_w/2, m_y + m_h - 70, White, 30, LANGUAGEPHRASE("dialog_vote_failed") );
            g_renderer->SetFont();
        }
    }       
}


// ============================================================================


class RequestCeasefireButton : public EclButton
{
public:
    int m_teamIndex;
    void Render( int realX, int realY, bool highlighted, bool clicked )
    {
        AlliancesWindow *parent = (AlliancesWindow *)m_parent;
        int teamId = parent->m_teamOrder[m_teamIndex];
        Team *team = g_app->GetWorld()->GetTeam(teamId);
        Team *myTeam = g_app->GetWorld()->GetMyTeam();

        if( team &&
            myTeam &&
            team != myTeam )
        {
            g_renderer2d->RectFill( realX, realY, m_w, m_h, Colour(10,10,50,100), true );
            g_renderer2d->Rect( realX, realY, m_w, m_h, Colour(255,255,255,200));

            if( highlighted || clicked )
            {
                g_renderer2d->RectFill( realX, realY, m_w, m_h, Colour(255,255,255,55), true );
            }

            if( myTeam->m_ceaseFire[teamId] )
            {
                Image *img = g_resource->GetImage( "gui/tick.bmp" );
                g_renderer->SetBlendMode( Renderer::BlendModeAdditive );
                g_renderer2d->StaticSprite( img, realX + 2, realY+2, 15, 15, White, true );
                g_renderer->SetBlendMode( Renderer::BlendModeNormal );

            }
        }
    }

    void MouseUp()
    {
        AlliancesWindow *parent = (AlliancesWindow *)m_parent;
        int teamId = parent->m_teamOrder[m_teamIndex];
        Team *team = g_app->GetWorld()->GetTeam(teamId);
        Team *myTeam = g_app->GetWorld()->GetMyTeam();

        if( team && 
            myTeam &&
            team != myTeam &&
            !EclGetWindow( "Cease Fire" ) )
        {
            EclRegisterWindow( new CeaseFireWindow( teamId ), m_parent );
        }
    }
};


class RequestShareRadarButton : public EclButton
{
public:
    int m_teamIndex;
    void Render( int realX, int realY, bool highlighted, bool clicked )
    {
        AlliancesWindow *parent = (AlliancesWindow *)m_parent;
        int teamId = parent->m_teamOrder[m_teamIndex];
        Team *team = g_app->GetWorld()->GetTeam(teamId);
        Team *myTeam = g_app->GetWorld()->GetMyTeam();

        if( team &&
            team != myTeam )
        {
            g_renderer2d->RectFill( realX, realY, m_w, m_h, Colour(10,10,50,100), true );
            g_renderer2d->Rect( realX, realY, m_w, m_h, Colour(255,255,255,200));

            if( highlighted || clicked )
            {
                g_renderer2d->RectFill( realX, realY, m_w, m_h, Colour(255,255,255,55), true );
            }

            if( myTeam->m_sharingRadar[teamId] )
            {
                Image *img = g_resource->GetImage( "gui/tick.bmp" );
                g_renderer->SetBlendMode( Renderer::BlendModeAdditive );
                g_renderer2d->StaticSprite( img, realX + 2, realY+2, 15, 15, White, true );
                g_renderer->SetBlendMode( Renderer::BlendModeNormal );                
            }
        }
    }

    void MouseUp()
    {
        AlliancesWindow *parent = (AlliancesWindow *)m_parent;
        int teamId = parent->m_teamOrder[m_teamIndex];
        Team *team = g_app->GetWorld()->GetTeam(teamId);
        Team *myTeam = g_app->GetWorld()->GetMyTeam();

        if( team && 
            team != myTeam &&
            !EclGetWindow( "Share Radar" ) )
        {
            EclRegisterWindow( new ShareRadarWindow( teamId ), m_parent );
        }
    }
};


class AllianceTeamButton : public EclButton
{
public:
    int m_teamIndex;
    void Render( int realX, int realY, bool highlighted, bool clicked )
    {
        AlliancesWindow *parent = (AlliancesWindow *)m_parent;
        int teamId = parent->m_teamOrder[m_teamIndex];
        int myTeamId = g_app->GetWorld()->m_myTeamId;
        Team *team = g_app->GetWorld()->GetTeam(teamId);
        if( team )
        {
            Colour teamCol = team->GetTeamColour();
            teamCol.m_a = 178;
            
            if( highlighted || clicked ) teamCol.m_a = 255;            

            Colour teamColDark = teamCol;
            teamColDark.m_r *= 0.2f;
            teamColDark.m_g *= 0.2f;
            teamColDark.m_b *= 0.2f;

            g_renderer2d->RectFill( realX, realY, m_w, m_h, teamCol, teamColDark, teamColDark, teamCol, true );
            g_renderer2d->Rect( realX, realY, m_w, m_h, Colour(255,255,255,100) );
            
            g_renderer2d->RectFill( realX + m_w-180, realY, 60, m_h, Colour(0,0,0,50), true );
            g_renderer2d->RectFill( realX + m_w-60, realY, 60, m_h, Colour(0,0,0,50), true );

            if( teamId == g_app->GetWorld()->m_myTeamId )
            {
                g_renderer2d->Rect( realX, realY, m_w, m_h, Colour(255,255,255,255) );
                g_renderer2d->TextCentreSimple( realX+180, realY+10, White, 15, "-" );
                g_renderer2d->TextCentreSimple( realX+240, realY+10, White, 15, "-" );
                g_renderer2d->TextCentreSimple( realX+305, realY+10, White, 15, "-" );
            }
            else
            {
                Image *img = g_resource->GetImage( "gui/tick.bmp" );
                g_renderer->SetBlendMode( Renderer::BlendModeAdditive );

                if( myTeamId != -1 &&
                    team->m_sharingRadar[myTeamId] )
                {
                    g_renderer2d->StaticSprite( img, realX+175, realY+8, 15, 15, White, true );
                }           

                int sharingRadar = g_app->GetGame()->GetOptionValue("RadarSharing");
                if( sharingRadar != 2 &&
                    g_app->GetWorld()->GetMyTeam() &&
                    g_app->GetWorld()->GetMyTeam()->m_sharingRadar[teamId] )                    
                {
                    g_renderer2d->StaticSprite( img, realX+235, realY+8, 15, 15, White, true );
                }

                g_renderer->SetBlendMode( Renderer::BlendModeNormal );
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
        AlliancesWindow *parent = (AlliancesWindow *)m_parent;
        int teamId = parent->m_teamOrder[m_teamIndex];
        parent->m_selectionTeamId = teamId;
    }
};


class AllianceVoteButton : public EclButton
{
public:
    int m_voteIndex;
    void Render( int realX, int realY, bool highlighted, bool clicked )
    {
        AlliancesWindow *parent = (AlliancesWindow *)m_parent;
        int voteId = parent->m_votes[m_voteIndex];
        
        Vote *vote = g_app->GetWorld()->m_votingSystem.LookupVote( voteId );
        Team *myTeam = g_app->GetWorld()->GetMyTeam();
        
        if( vote )
        {
            g_renderer2d->RectFill( realX, realY, m_w, m_h, Colour(10,10,50,200), true );
            
            Colour borderCol(255,255,255,100);
            if( highlighted || clicked ) 
            {
                borderCol.m_a = 255;
                g_renderer2d->RectFill( realX, realY, m_w, m_h, Colour(100,100,150,100), true );
            }
            g_renderer2d->Rect( realX, realY, m_w, m_h, borderCol );

            switch( vote->m_voteType )
            {
                case Vote::VoteTypeLeaveAlliance:
                    if( myTeam->m_teamId == vote->m_createTeamId )
                    {
                        const char *p1 = g_languageTable->DoesPhraseExist("dialog_you_requested_leave_1") ? LANGUAGEPHRASE("dialog_you_requested_leave_1") : "You have requested to";
                        const char *p2 = g_languageTable->DoesPhraseExist("dialog_you_requested_leave_2") ? LANGUAGEPHRASE("dialog_you_requested_leave_2") : "leave your Alliance";
                        g_renderer2d->TextSimple( realX+10, realY+5, White, 14, p1 );
                        g_renderer2d->TextSimple( realX+10, realY+20, White, 14, p2 );
                    }
                    else if( myTeam->m_allianceId == g_app->GetWorld()->GetTeam( vote->m_createTeamId )->m_allianceId )
                    {
                        Team *team = g_app->GetWorld()->GetTeam( vote->m_createTeamId );
                        char caption[512];
                        strcpy( caption, g_languageTable->DoesPhraseExist("dialog_requested_leave_1") ? LANGUAGEPHRASE("dialog_requested_leave_1") : "*T has requested to" );
                        LPREPLACESTRINGFLAG( 'T', team->m_name, caption );
                        g_renderer2d->TextSimple( realX+10, realY+5, White, 15, caption );
                        const char *p2 = g_languageTable->DoesPhraseExist("dialog_requested_leave_2") ? LANGUAGEPHRASE("dialog_requested_leave_2") : "leave the Alliance";
                        g_renderer2d->TextSimple( realX+10, realY+20, White, 14, p2 );
                    }
                    break;

                case Vote::VoteTypeMigrateAlliance:
                    if( myTeam->m_teamId == vote->m_createTeamId )
                    {
                        const char *allianceName = g_app->GetWorld()->GetAllianceName(vote->m_voteData);
                        const char *p1 = g_languageTable->DoesPhraseExist("dialog_you_req_migrate_alliance_1") ? LANGUAGEPHRASE("dialog_you_req_migrate_alliance_1") : "You requested to";
                        g_renderer2d->TextSimple( realX+10, realY+5, White, 14, p1 );
                        char caption[512];
                        strcpy( caption, g_languageTable->DoesPhraseExist("dialog_you_req_migrate_alliance_2") ? LANGUAGEPHRASE("dialog_you_req_migrate_alliance_2") : "change your Alliance color to *A" );
                        LPREPLACESTRINGFLAG( 'A', allianceName, caption );
                        g_renderer2d->TextSimple( realX+10, realY+20, White, 14, caption );
                    }
                    else if( myTeam->m_allianceId == g_app->GetWorld()->GetTeam( vote->m_createTeamId )->m_allianceId )
                    {
                        const char *allianceName = g_app->GetWorld()->GetAllianceName(vote->m_voteData);
                        Team *team = g_app->GetWorld()->GetTeam( vote->m_createTeamId );
                        char caption[512];
                        strcpy( caption, g_languageTable->DoesPhraseExist("dialog_requested_migrate_alliance_1") ? LANGUAGEPHRASE("dialog_requested_migrate_alliance_1") : "*T requested to" );
                        LPREPLACESTRINGFLAG( 'T', team->m_name, caption );
                        g_renderer2d->TextSimple( realX+10, realY+5, White, 15, caption );
                        strcpy( caption, g_languageTable->DoesPhraseExist("dialog_requested_migrate_alliance_2") ? LANGUAGEPHRASE("dialog_requested_migrate_alliance_2") : "change Alliance color to *A" );
                        LPREPLACESTRINGFLAG( 'A', allianceName, caption );
                        g_renderer2d->TextSimple( realX+10, realY+20, White, 14, caption );
                    }
                    break;

                case Vote::VoteTypeJoinAlliance:
                    if( myTeam->m_teamId == vote->m_createTeamId )
                    {
                        const char *allianceName = g_app->GetWorld()->GetAllianceName(vote->m_voteData);
                        g_renderer2d->TextSimple( realX+10, realY+5, White, 14, LANGUAGEPHRASE("dialog_you_requested_alliance_1") );
						char caption[512];
                        strcpy( caption, LANGUAGEPHRASE("dialog_you_requested_alliance_2") );
						LPREPLACESTRINGFLAG( 'A', allianceName, caption );
                        g_renderer2d->TextSimple( realX+10, realY+20, White, 14, caption );
                    }
                    else 
                    {
                        Team *team = g_app->GetWorld()->GetTeam( vote->m_createTeamId );
						char caption[512];
                        strcpy( caption, LANGUAGEPHRASE("dialog_requested_alliance_1") );
						LPREPLACESTRINGFLAG( 'T', team->m_name, caption );
                        g_renderer2d->TextSimple( realX+10, realY+5, White, 15, caption );
                        g_renderer2d->TextSimple( realX+10, realY+20, White, 15, LANGUAGEPHRASE("dialog_requested_alliance_2") );
                    }
                    break;

                case Vote::VoteTypeKickPlayer:
                {
                    Team *kickTeam = g_app->GetWorld()->GetTeam(vote->m_voteData);
                    if( myTeam->m_teamId == vote->m_createTeamId )
                    {
                        g_renderer2d->TextSimple( realX+10, realY+5, White, 15, LANGUAGEPHRASE("dialog_you_requested_kick_1") );
						char caption[512];
                        strcpy( caption, LANGUAGEPHRASE("dialog_you_requested_kick_2") );
						LPREPLACESTRINGFLAG( 'T', kickTeam->m_name, caption );
                        g_renderer2d->TextSimple( realX+10, realY+20, White, 15, caption );
                    }
                    else
                    {
                        Team *team = g_app->GetWorld()->GetTeam( vote->m_createTeamId );
						char caption[512];
                        strcpy( caption, LANGUAGEPHRASE("dialog_requested_kick_1") );
						LPREPLACESTRINGFLAG( 'T', team->m_name, caption );
                        g_renderer2d->TextSimple( realX+10, realY+5, White, 15, caption );
                        strcpy( caption, LANGUAGEPHRASE("dialog_requested_kick_2") );
						LPREPLACESTRINGFLAG( 'T', kickTeam->m_name, caption );
                        g_renderer2d->TextSimple( realX+10, realY+20, White, 15, caption );
                    }
                    break;
                }
            }

            int yes, no, abstain;
            vote->GetCurrentVote( &yes, &no, &abstain );

            g_renderer2d->Text( realX + 190, realY + 12, White, 17, "%ds", int(vote->m_timer) );
            
            g_renderer2d->TextSimple( realX + m_w - 70, realY + 5, White, 10,  LANGUAGEPHRASE("dialog_vote_yes") );
            g_renderer2d->TextSimple( realX + m_w - 70, realY + 15, White, 10, LANGUAGEPHRASE("dialog_vote_no") );
            g_renderer2d->TextSimple( realX + m_w - 70, realY + 25, White, 10, LANGUAGEPHRASE("dialog_vote_abstain") );

            g_renderer2d->Text( realX + m_w - 20, realY + 5, White, 10,  "%d", yes );
            g_renderer2d->Text( realX + m_w - 20, realY + 15, White, 10, "%d", no );
            g_renderer2d->Text( realX + m_w - 20, realY + 25, White, 10, "%d", abstain );
        }
    }

    void MouseUp()
    {
        bool isReplayMode = g_app->GetServer() && g_app->GetServer()->IsRecordingPlaybackMode();
        int isSpectator = g_app->GetWorld()->IsSpectating( g_app->GetClientToServer()->m_clientId );
        
        if( isReplayMode || isSpectator != -1 ) return;
        
        AlliancesWindow *parent = (AlliancesWindow *)m_parent;
        int voteId = parent->m_votes[m_voteIndex];
        Vote *vote = g_app->GetWorld()->m_votingSystem.LookupVote( voteId );
                
        if( vote )
        {
            VotingWindow *votingWindow = new VotingWindow();
            votingWindow->m_voteId = voteId;
            EclRegisterWindow( votingWindow, m_parent );
        }
    }
};



AlliancesWindow::AlliancesWindow()
:   InterfaceWindow("Alliances", "dialog_alliances", true),
    m_selectionTeamId(-1)
{
    int numTeams = g_app->GetWorld()->m_teams.Size();

    SetSize( 600, 120+numTeams * 35 );
    SetPosition( g_windowManager->WindowW()/2 - m_w - 10,
                 g_windowManager->WindowH()/2 - m_h/2 );

    for( int i = 0; i < MAX_TEAMS; ++i )
    {
        m_teamOrder[i] = -1;
        m_votes[i] = -1;
    }    

    m_selectionTeamId = g_app->GetWorld()->m_myTeamId;
}


void AlliancesWindow::Create()
{
    InterfaceWindow::Create();

    float xPos = 20;
    float yPos = 60;
    float width = m_w - 270;
    float height = 30;
    float gap = 7;

    int permitDefection = g_app->GetGame()->GetOptionValue( "PermitDefection" );
    int sharingRadar = g_app->GetGame()->GetOptionValue("RadarSharing");
    

    //
    // Invert box

    InvertedBox *box = new InvertedBox();
    box->SetProperties( "box", xPos-10, yPos-10, width+20, (height+gap) * g_app->GetWorld()->m_teams.Size() + 15, " ", " ", false, false );
    RegisterButton( box );


    //
    // Buttons for each team

    for( int i = 0; i < g_app->GetWorld()->m_teams.Size(); ++i )
    {        
        char name[256];
        sprintf( name, "Team %d", i );

        AllianceTeamButton *team = new AllianceTeamButton();
        team->SetProperties( name, xPos, yPos, width, height, " ", " ", false, false );
        team->m_teamIndex = i;
        RegisterButton( team );

        if( permitDefection )
        {
            sprintf( name, "ceasfire %d", i );
            RequestCeasefireButton *ceasefire = new RequestCeasefireButton();
            ceasefire->SetProperties( name, xPos + 295, yPos+5, 18, 18, " ", " ", false, false );
            ceasefire->m_teamIndex = i;
            RegisterButton( ceasefire );
        }

        if( sharingRadar == 2 )
        {
            sprintf( name, "shareradar %d", i );
            RequestShareRadarButton *shareRadar = new RequestShareRadarButton();
            shareRadar->SetProperties( name, xPos + 235, yPos+5, 18, 18, " ", " ", false, false );
            shareRadar->m_teamIndex = i;
            RegisterButton( shareRadar );
        }
       
        yPos += height;
        yPos += gap;
    }

    yPos += gap * 2;


    //
    // Join / Leave / Kick button
    
    if( permitDefection )
    {
        JoinLeaveButton *joinLeave = new JoinLeaveButton();
        joinLeave->SetProperties( "JoinLeave", xPos+width/2 - 120, yPos, 240, 23, " ", " ", false, false );
        RegisterButton( joinLeave );
    }


    yPos += 25;
    yPos += gap;
    height = 40;


    //
    // Close button

    CloseAlliancesButton *close = new CloseAlliancesButton();
    close->SetProperties( "Close", m_w - 110, m_y - 25, 100, 18, "dialog_close", " ", true, false );
    RegisterButton( close );


    //
    // Once button for each vote

    for( int i = 0; i < MAX_TEAMS; ++i )
    {
        char name[256];
        sprintf( name, "Vote %d", i );

        AllianceVoteButton *vote = new AllianceVoteButton();
        vote->SetProperties( name, xPos, yPos, width, height, " ", " ", false, false );
        vote->m_voteIndex = i;
        RegisterButton( vote );

        yPos += height;
        yPos += gap;
    }
}


void AlliancesWindow::Update()
{   
    //
    // Build a list of all teams;

    LList<Team *> teams;
    for( int i = 0; i < g_app->GetWorld()->m_teams.Size(); ++i )
    {
        Team *team = g_app->GetWorld()->m_teams[i];
        teams.PutData( team );
    }


    //
    // Now put the other teams in, in alliance order

    int currentIndex = 0;

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

    //
    // Are there any votes we can see?
    
    for( int i = 0; i < MAX_TEAMS; ++i )
    {
        m_votes[i] = -1;
    }    

    currentIndex = 0;
    
    for( int i = 0; i < g_app->GetWorld()->m_votingSystem.m_votes.Size(); ++i )
    {
        if( g_app->GetWorld()->m_votingSystem.m_votes.ValidIndex(i) )
        {
            Vote *vote = g_app->GetWorld()->m_votingSystem.m_votes[i];
            if( vote->m_result == Vote::VoteUnknown &&
                vote->CanSeeVote( g_app->GetWorld()->m_myTeamId ) )
            {
                m_votes[currentIndex] = i;
                ++currentIndex;
            }
        }
    }


    //
    // Make sure we are the right size
   
    m_h = 120 + g_app->GetWorld()->m_teams.Size() * 42;
    m_h += currentIndex * 45;    

    if( currentIndex > 0 ) m_h += 10;
    
    m_h = max(m_h, 300 );

    GetButton("Close")->m_y = m_h - 25;
}


void AlliancesWindow::Render( bool _hasFocus )
{
    InterfaceWindow::Render( _hasFocus );

    g_renderer2d->TextSimple( m_x + 30, m_y + 25, White, 20, LANGUAGEPHRASE("dialog_alliance_player") );

    g_renderer2d->TextCentreSimple( m_x+200, m_y+22, White, 12, LANGUAGEPHRASE("dialog_alliance_receiving") );
    g_renderer2d->TextCentreSimple( m_x+200, m_y+34, White, 12, LANGUAGEPHRASE("dialog_alliance_radar") );

    g_renderer2d->TextCentreSimple( m_x+260, m_y+22, White, 12, LANGUAGEPHRASE("dialog_alliance_sending") );
    g_renderer2d->TextCentreSimple( m_x+260, m_y+34, White, 12, LANGUAGEPHRASE("dialog_alliance_radar") );

    g_renderer2d->TextCentreSimple( m_x+320, m_y+22, White, 12, LANGUAGEPHRASE("dialog_alliance_cease") );
    g_renderer2d->TextCentreSimple( m_x+320, m_y+34, White, 12, LANGUAGEPHRASE("dialog_alliance_fire") );


    GameOption *defection = g_app->GetGame()->GetOption("PermitDefection");
    GameOption *radarShare = g_app->GetGame()->GetOption("RadarSharing");

    float xPos = m_x + m_w - 230;
    float yPos = m_y + 25;

    //
    // Render the main rules

    g_renderer2d->Text( xPos, yPos, White, 14, "%s :", GameOption::TranslateValue( defection->m_name ) );
    g_renderer2d->Text( xPos+160, yPos, White, 14, "[%s]", GameOption::TranslateValue( defection->m_subOptions[defection->m_currentValue] ) );

    g_renderer2d->Text( xPos, yPos+=17, White, 14, "%s :", GameOption::TranslateValue( radarShare->m_name ) );
    g_renderer2d->Text( xPos+160, yPos, White, 14, "[%s]", GameOption::TranslateValue( radarShare->m_subOptions[radarShare->m_currentValue] ) );


    //
    // Defection help

    char defectionStringId[256];
    sprintf( defectionStringId, "tooltip_defection_%s", defection->m_subOptions[defection->m_currentValue] );
    strlwr( defectionStringId ); 
    
    if( g_languageTable->DoesPhraseExist( defectionStringId ) )
    {
        yPos += 15;

        const char *fullString = LANGUAGEPHRASE(defectionStringId);

        MultiLineText wrapped( fullString, 210, 10 );

        for( int i = 0; i < wrapped.Size(); ++i )
        {
            char *thisString = wrapped[i];
            g_renderer2d->TextSimple( xPos, yPos+=12, White, 10, thisString );
        }
    }

    //
    // Radar sharing help

    sprintf( defectionStringId, "tooltip_radarsharing_%s", radarShare->m_subOptions[radarShare->m_currentValue] );
    strlwr( defectionStringId ); 

    if( g_languageTable->DoesPhraseExist( defectionStringId ) )
    {
        yPos += 15;

        const char *fullString = LANGUAGEPHRASE(defectionStringId);

        MultiLineText wrapped( fullString, 210, 10 );

        for( int i = 0; i < wrapped.Size(); ++i )
        {
            char *thisString = wrapped[i];
            g_renderer2d->TextSimple( xPos, yPos+=12, White, 10, thisString );
        }
    }

}


// ============================================================================

class TeamAllianceColorButton : public InterfaceButton
{
public:
    int m_allianceId;

    void Render( int realX, int realY, bool highlighted, bool clicked )
    {
        Team *myTeam = g_app->GetWorld()->GetMyTeam();
        if( !myTeam ) return;

        if( myTeam->m_allianceId == m_allianceId )
        {
            g_renderer2d->RectFill( realX, realY, m_w-10, m_h, Colour(100,100,200,200) );
            g_renderer2d->Rect( realX, realY, m_w-10, m_h, Colour(255,255,255,155) );
        }
        else if( g_app->GetWorld()->CountAllianceMembers(m_allianceId) >= 1 )
        {
            g_renderer2d->RectFill( realX, realY, m_w-10, m_h, Colour(0,0,0,200) );
            g_renderer2d->Rect( realX, realY, m_w-10, m_h, Colour(100,100,100,155) );
        }

        if( highlighted || clicked )
        {
            g_renderer2d->RectFill( realX, realY, m_w, m_h, Colour(100,100,150,100) );
        }

        const char *captionText = m_caption ? ( m_captionIsLanguagePhrase ? LANGUAGEPHRASE(m_caption) : m_caption ) : "";
        g_renderer2d->TextSimple( realX + 5, realY + m_h/2 - 6, White, 12, captionText );

        Colour allianceCol = g_app->GetWorld()->GetAllianceColour( m_allianceId );
        allianceCol.m_a = 255;
        Colour darker = allianceCol;
        darker.m_r *= 0.3f;
        darker.m_g *= 0.3f;
        darker.m_b *= 0.3f;

        float colourX = realX + m_w - 50;
        float colourY = realY + 3;
        float colourW = 45;
        float colourH = m_h - 6;

        g_renderer2d->RectFill( colourX, colourY, colourW, colourH, allianceCol, darker, false );
        g_renderer2d->Rect( colourX, colourY, colourW, colourH, Colour(255,255,255,55) );
    }

    void MouseUp()
    {
        if( g_app->GetWorld()->CountAllianceMembers(m_allianceId) == 0 )
        {
            g_app->GetClientToServer()->BeginVote( g_app->GetWorld()->m_myTeamId, Vote::VoteTypeMigrateAlliance, m_allianceId );
            EclRemoveWindow( m_parent->m_name );
            EclRemoveWindow( "Alliances" );
        }
    }
};


ColorPickWindow::ColorPickWindow()
:   InterfaceWindow("ALLIANCE COLOR", g_languageTable->DoesPhraseExist("dialog_pick_color") ? "dialog_pick_color" : "Pick Alliance Color", true),
    m_selectionTeamId(-1)
{
    int colorsPerColumn = 14;
    float buttonHeight = 30;
    float windowHeight = 120 + colorsPerColumn * buttonHeight;
    float windowWidth = 175 * 2 + 20;

    SetSize( windowWidth, windowHeight );
    SetPosition( g_windowManager->WindowW()/2 + 10,
                 g_windowManager->WindowH()/2 - m_h/2 );

    for( int i = 0; i < MAX_TEAMS; ++i )
    {
        m_teamOrder[i] = -1;
        m_votes[i] = -1;
    }

    m_selectionTeamId = g_app->GetWorld()->m_myTeamId;
}


void ColorPickWindow::Create()
{
    InterfaceWindow::Create();

    float xPos = 10;
    float yPos = 30;
    float buttonWidth = 175;
    float buttonHeight = 30;
    float columnGap = 20;
    int colorsPerColumn = 14;

    for( int i = 0; i < 28; ++i )
    {
        int column = i / colorsPerColumn;
        int row = i % colorsPerColumn;

        float x2Pos = xPos + column * (buttonWidth + columnGap);
        float y2Pos = yPos + row * buttonHeight;
        float w = buttonWidth;

        TeamAllianceColorButton *rtb = new TeamAllianceColorButton();
        rtb->m_allianceId = i;
        char name[64];
        sprintf( name, "alliance %d", i + 1 );

        char caption[256];
        strcpy( caption, LANGUAGEPHRASE("dialog_join_alliance") );
        LPREPLACESTRINGFLAG( 'A', g_app->GetWorld()->GetAllianceName(i), caption );

        rtb->SetProperties( name, x2Pos, y2Pos, w-10, buttonHeight, caption, " ", false, false );
        RegisterButton( rtb );
    }

    CloseButton *close = new CloseButton();
    close->SetProperties( "Close", m_w/2 - 50, m_h - 30, 100, 18, "dialog_close", " ", true, false );
    RegisterButton( close );
}


void ColorPickWindow::Update()
{
    for( int i = 0; i < MAX_TEAMS; ++i )
    {
        m_teamOrder[i] = -1;
        m_votes[i] = -1;
    }
}


void ColorPickWindow::Render( bool _hasFocus )
{
    InterfaceWindow::Render( _hasFocus );
}


