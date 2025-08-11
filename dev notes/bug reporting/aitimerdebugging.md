# Code Snippets

## hard coding timer fixes rng divergence
### team.cpp

```
void Team::RunAI()
{
    if( g_app->GetServer()->IsRecordingPlaybackMode() )
    {
        static bool s_timerAdjusted = false;
        if( !s_timerAdjusted )
        {
            m_aiActionTimer = 4.56f; // SOMEHOW THIS WORKS
            s_timerAdjusted = true;
        }
    }
```
## changes sync validation, but this helped me determine that the ai timers were different between playing the recording through dedcon and replay viewer
### team.cpp

```
void Team::RunAI()
{
    START_PROFILE("TeamAI");
    if( g_app->GetWorld()->GetTimeScaleFactor() == 0 )
    {
        END_PROFILE("TeamAI");
        return;
    }

    // *** LOG AI RNG USAGE ***
    static int s_aiCallCount = 0;
    static int s_lastSyncValue = 0;
    
    if( m_type == Team::TypeAI )
    {
        int currentSyncValue = _syncrand() % 1000;  
        AppDebugOut("AI[%d] call#%d: syncrand=%d, state=%d, aiActionTimer=%.2f\n", 
                   m_teamId, s_aiCallCount++, currentSyncValue, m_currentState, m_aiActionTimer.DoubleValue());
        s_lastSyncValue = currentSyncValue;
    }
```

## useful for detecting the actual timer values when starting the replay server
### defcon.cpp

```
                if( shouldAdvance )
                {
                    AppDebugOut("Server advance BEFORE: gameRunning=%s, seqId=%d, timeNow=%.2f\n", 
                               g_app->m_gameRunning ? "true" : "false", 
                               g_app->GetServer() ? g_app->GetServer()->m_sequenceId : -1,
                               timeNow);
                    
                    g_app->GetServer()->Advance();
                    float timeToAdd = SERVER_ADVANCE_PERIOD.DoubleValue();
                    if( !g_app->m_gameRunning ) timeToAdd *= 5.0f;
                    
                    AppDebugOut("Server advance AFTER: timeToAdd=%.2f, nextAdvanceTime will be %.2f\n", 
                               timeToAdd, g_nextServerAdvanceTime + timeToAdd);
```

## this helped me determine that the timescale factor was not incorrect in replay viewer servers
### world.cpp

```
    AppDebugOut("GetTimeScaleFactor called: returning %d, gameRunning=%s\n", 
               m_timeScaleFactor, g_app->m_gameRunning ? "true" : "false");
```

## this helped me figure out that ai timers are actually initialised inside the lobby, but only update when the game starts
### server.cpp

```
    if( m_sequenceId < 350 ) // Only log first 350 sequences to avoid spam
    {
        for( int i = 0; i < g_app->GetWorld()->m_teams.Size(); ++i )
        {
            Team *team = g_app->GetWorld()->m_teams[i];
            if( team && team->m_type == Team::TypeAI )
            {
                AppDebugOut("m_aiActionTimer Seq: %d Team: %d Value: %.2f\n", 
                           m_sequenceId, team->m_teamId, team->m_aiActionTimer.DoubleValue());
            }
        }
    }
```

## helped me determine that the world state itself was not different between dedcon and replay viewer
## recording_selection.cpp

```
    if( connectingWindowOpen ) EclRegisterWindow( new ConnectingWindow() );

    AppDebugOut("*** SEED CALC START ***\n");
    int randSeed = 0;
    for( int i = 0; i < m_world->m_teams.Size(); ++i )
    {
        AppDebugOut("Team %d: m_randSeed = %d\n", i, m_world->m_teams[i]->m_randSeed);
        randSeed += m_world->m_teams[i]->m_randSeed;
    }
    AppDebugOut("Final combined seed = %d\n", randSeed);
    syncrandseed( randSeed );

    GetWorld()->LoadNodes();
    GetWorld()->AssignCities();

    AppDebugOut("*** WORLD STATE DEBUG ***\n");
    AppDebugOut("Total cities: %d\n", g_app->GetWorld()->m_cities.Size());
    
    // Log team territories
    for( int i = 0; i < g_app->GetWorld()->m_teams.Size(); ++i )
    {
        Team *team = g_app->GetWorld()->m_teams[i];
        AppDebugOut("Team %d territories: %d\n", i, team->m_territories.Size());
        
        // Log first few territory IDs
        for( int j = 0; j < min(5, team->m_territories.Size()); ++j )
        {
            AppDebugOut("  Territory[%d] = %d\n", j, team->m_territories[j]);
        }
    }
    
    // Log first few cities and their team assignments
    for( int i = 0; i < min(10, g_app->GetWorld()->m_cities.Size()); ++i )
    {
        City *city = g_app->GetWorld()->m_cities[i];
        AppDebugOut("City[%d]: team=%d, pop=%d\n", i, city->m_teamId, city->m_population);
    }
```
