#include "lib/universal_include.h"

#include "RecordingServer.h"
#include "RecordingController.h"

#include "app/app.h"
#include "app/globals.h"
#include "network/Server.h"
#include "network/ClientToServer.h"
#include "interface/connecting_window.h"
#include "interface/interface.h"
#include "lib/eclipse/eclipse.h"
#include "lib/eclipse/components/message_dialog.h"
#include "lib/language_table.h"
#include "lib/netlib/net_lib.h"
#include "world/world.h"
#include "world/team.h"

RecordingServer::RecordingServer()
{
}

RecordingServer::~RecordingServer()
{
}

bool RecordingServer::CheckClientListenerReady()
{
    if (!g_app || !g_app->GetClientToServer())
    {
        return false;
    }

    return g_app->GetClientToServer()->m_listener != nullptr;
}

bool RecordingServer::StartPlayback(const std::string& filename, bool startFromLobby)
{
    if (!g_app)
    {
        AppDebugOut("RecordingServer: g_app is null\n");
        return false;
    }

    //
    // Check if ClientToServer listener is ready

    if (!CheckClientListenerReady())
    {
        char msgtext[2048];
#if defined(RETAIL_BRANDING_UK)
        snprintf(msgtext, sizeof(msgtext), "%s%s", LANGUAGEPHRASE("dialog_client_failedconnect"), LANGUAGEPHRASE("website_support_retail_uk"));
#elif defined(RETAIL_BRANDING)
        snprintf(msgtext, sizeof(msgtext), "%s%s", LANGUAGEPHRASE("dialog_client_failedconnect"), LANGUAGEPHRASE("website_support_retail"));
#elif defined(RETAIL_BRANDING_MULTI_LANGUAGE)
        snprintf(msgtext, sizeof(msgtext), "%s%s", LANGUAGEPHRASE("dialog_client_failedconnect"), LANGUAGEPHRASE("website_support_retail_multi_language"));
#elif defined(TARGET_OS_MACOSX)
        snprintf(msgtext, sizeof(msgtext), "%s%s", LANGUAGEPHRASE("dialog_client_failedconnect"), LANGUAGEPHRASE("website_support_macosx"));
#else
        snprintf(msgtext, sizeof(msgtext), "%s%s", LANGUAGEPHRASE("dialog_client_failedconnect"), LANGUAGEPHRASE("website_support"));
#endif
        msgtext[sizeof(msgtext) - 1] = '\0';

        MessageDialog *msg = new MessageDialog("NETWORK ERROR",
                                                msgtext, false,
                                                "dialog_client_failedtitle", true);
        EclRegisterWindow(msg);
        return false;
    }

    g_app->ShutdownCurrentGame();

    char ourIp[256];
    GetLocalHostIP(ourIp, sizeof(ourIp));

    bool success = g_app->InitServer();
    if (!success)
    {
        return false;
    }

    //
    // Start recording playback server

    bool recordingLoaded = g_app->GetServer()->StartRecordingPlaybackServer(filename);
    if (!recordingLoaded)
    {
        g_app->ShutdownCurrentGame();
        
        return false;
    }

    g_app->InitWorld();

    if (!startFromLobby)
    {
        int gameStartSeqId = g_app->GetServer()->m_playbackController->GetStartSeqId();
        if (gameStartSeqId > 0)
        {
            g_app->GetServer()->EnableSeeking(gameStartSeqId);
        }
    }

    int ourPort = g_app->GetServer()->GetLocalPort();
    g_app->GetClientToServer()->ClientJoin(ourIp, ourPort);

    //
    // Create connecting window

    ConnectingWindow *connectingWindow = new ConnectingWindow();
    connectingWindow->m_popupLobbyAtEnd = true;

    if (!startFromLobby)
    {
        int gameStartSeqId = g_app->GetServer()->m_playbackController->GetStartSeqId();
        connectingWindow->SetFastForwardMode(true, gameStartSeqId);
    }

    EclRegisterWindow(connectingWindow);

    return true;
}

bool RecordingServer::CreateFakeDisconnect(Team *team)
{
    if (!team || !g_app->GetWorld())
    {
        return false;
    }

    //
    // Only convert human teams

    if (team->m_type != Team::TypeLocalPlayer && team->m_type != Team::TypeRemotePlayer)
    {
        return false;
    }

    //
    // Store team names

    char teamName[256];
    strncpy(teamName, team->m_name, sizeof(teamName) - 1);
    teamName[sizeof(teamName) - 1] = '\0';

    team->m_clientId = -1;
    team->SetTeamType(Team::TypeAI);

    //
    // Show world message

    char worldMsg[256];
    strcpy(worldMsg, LANGUAGEPHRASE("dialog_world_disconnected"));
    LPREPLACESTRINGFLAG('T', teamName, worldMsg);
    strupr(worldMsg);
    g_app->GetInterface()->ShowMessage(0, 0, -1, worldMsg, true);

    //
    // Add chat message

    char chatMsg[256];
    strcpy(chatMsg, LANGUAGEPHRASE("dialog_world_team_left_game"));
    LPREPLACESTRINGFLAG('T', teamName, chatMsg);
    g_app->GetWorld()->AddChatMessage(-1, 100, chatMsg, -1, false);

    return true;
}