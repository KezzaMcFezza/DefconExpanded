#include "lib/universal_include.h"

#include "RecordingServer.h"
#include "RecordingController.h"

#include "app/app.h"
#include "app/globals.h"
#include "network/Server.h"
#include "network/ClientToServer.h"
#include "interface/connecting_window.h"
#include "lib/eclipse/eclipse.h"
#include "lib/eclipse/components/message_dialog.h"
#include "lib/language_table.h"
#include "lib/netlib/net_lib.h"

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
            g_app->GetServer()->EnableFastForward(gameStartSeqId, 500.0f);
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