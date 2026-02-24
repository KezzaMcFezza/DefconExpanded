#include "lib/universal_include.h"


#include "lib/eclipse/eclipse.h"
#include "lib/gucci/input.h"
#include "lib/gucci/window_manager.h"
#include "lib/resource/resource.h"
#include "lib/resource/image.h"
#include "lib/render/renderer.h"
#include "lib/render2d/renderer_2d.h"
#include "lib/render3d/renderer_3d.h"
#include "lib/render2d/megavbo/megavbo_2d.h"
#include "lib/render/colour.h"
#include "lib/render/styletable.h"
#include "lib/debug/profiler.h"
#include "lib/language_table.h"
#include "lib/preferences.h"
#include "lib/math/math_utils.h"
#include "lib/math/random_number.h"
#include "lib/hi_res_time.h"
#include "lib/sound/soundsystem.h"
#include "lib/resource/image.h"
#include "lib/resource/bitmap.h"

#include "app/app.h"
#include "app/globals.h"
#include "lib/multiline_text.h"
#include "app/game.h"
#ifdef TARGET_OS_MACOSX
#include "app/macutil.h"
#endif

#include "network/ClientToServer.h"

#include "interface/interface.h"

#include "renderer/world_renderer.h"
#include "renderer/map_renderer.h"
#include "renderer/globe_renderer.h"
#include "renderer/animated_icon.h"

#include "world/world.h"
#include "world/earthdata.h"
#include "world/worldobject.h"
#include "world/explosion.h"
#include "world/city.h"
#include "world/silo.h"
#include "world/nuke.h"
#include "world/node.h"
#include "world/blip.h"
#include "world/fleet.h"
#include "world/whiteboard.h"
#include "curves.h"


MapRenderer::MapRenderer()
:   m_middleX(0.0f),
    m_middleY(0.0f),
    m_zoomFactor(1),
    m_seamIteration(0),
    m_showNodes(true),
    bmpRadar(NULL),
    m_highlightTime(0.0f),
    m_totalZoom(0),
    m_oldMouseX(0.0f),
    m_oldMouseY(0.0f),
    m_mouseIdleTime(0.0f),
    m_stateRenderTime(0.0f),
    m_stateObjectId(-1),
    m_stateApplyToSelectionSameType(false),
    m_highlightUnit(-1),
    m_showAllTeams(false),
    m_cameraLongitude(0.0f),
    m_cameraLatitude(0.0f),
    m_speedRatioX(0.0f),
    m_speedRatioY(0.0f),
    m_cameraIdleTime(0.0f),
    m_cameraZoom(0),
    m_autoCam(false),
    m_autoCamCounter(0),
    m_camSpeed(0),
    m_camAtTarget(true),
    m_autoCamTimer(20.0f),
    m_lockCamControl(false),
    m_lockCommands(false),
    m_draggingCamera(false),
    m_isDraggingSelect(false),
    m_dragSelectMode(0),
    m_dragStartLong(0),
    m_dragStartLat(0),
    m_dragEndLong(0),
    m_dragEndLat(0),
    m_lastRightClickTime(0.0f),
    m_lastRightClickX(0.0f),
    m_lastRightClickY(0.0f),
    m_tooltip(NULL),
    m_tooltipTimer(0.0f),
	m_longitudePlanningOld(0.0f),
	m_latitudePlanningOld(0.0f)
{
    for( int i = 0; i < MAX_TEAMS; ++i )
    {
        m_showTeam[i] = false;
    }
}

MapRenderer::~MapRenderer()
{
    Reset();
}

void MapRenderer::Init()
{
    Reset();

    bmpRadar        = g_resource->GetImage( "graphics/radar.bmp" );
    bmpBlur         = g_resource->GetImage( "graphics/blur.bmp" );
    bmpWater        = g_resource->GetImage( "graphics/water.bmp" );
    bmpExplosion    = g_resource->GetImage( "graphics/explosion.bmp" );
    bmpPopulation    = g_resource->GetImage( "graphics/population.bmp" );
}


void MapRenderer::Reset()
{
    g_app->GetWorldRenderer()->Reset();

    m_isDraggingSelect = false;
    m_stateApplyToSelectionSameType = false;

    delete m_tooltip;
    m_tooltip = NULL;
}


void MapRenderer::Render()
{
    START_PROFILE( "MapRenderer" );

    //
    // Render it!

    float mapColourFader = 1.0f;
    Team *myTeam = g_app->GetWorld()->GetMyTeam();
    if( myTeam ) mapColourFader = myTeam->m_teamColourFader;

    float popupScale = g_preferences->GetFloat(PREFS_INTERFACE_POPUPSCALE);
    m_drawScale = m_zoomFactor / (1.0f-popupScale*0.1f);

    float resScale = g_windowManager->WindowH() / 800.0f;
    m_drawScale /= resScale;

#ifdef _DEBUG
    if( myTeam && myTeam->m_type == Team::TypeAI )
    {
        g_renderer2d->Text( 10, 10, White, 13, "Visible = %d", myTeam->m_targetsVisible );
        g_renderer2d->Text( 10, 30, White, 13, "MaxVisible = %d", myTeam->m_maxTargetsSeen );

        const char *state = "";
        switch( myTeam->m_currentState )
        {
            case Team::StatePlacement:      state = "Placement";    break;
            case Team::StateScouting:       state = "Scouting";     break;
            case Team::StateAssault:        state = "Assault";      break;
            case Team::StateStrike:         state = "Strike";       break;
            case Team::StatePanic:          state = "Panic";        break;
            case Team::StateFinal:          state = "Final";        break;
        }

        g_renderer2d->TextSimple( 10, 50, White, 13, state );
    }
#endif

    float left, right, top, bottom;
    GetWindowBounds( &left, &right, &top, &bottom );

    // Render the landscape first to avoid object clipping at map edges
    for( int x = 0; x < 2; ++x )
    {
        g_renderer2d->Set2DViewport( left, right, bottom, top, m_pixelX, m_pixelY, m_pixelW, m_pixelH );
        Colour blurColour = g_styleTable->GetPrimaryColour( STYLE_WORLD_LAND );
        Colour desaturatedColour = g_styleTable->GetSecondaryColour( STYLE_WORLD_LAND );
        blurColour = blurColour * mapColourFader + desaturatedColour * (1-mapColourFader);
        float lineWidth = 1.0f;

        g_renderer2d->BeginStaticSpriteBatch();
        g_renderer2d->BeginLineBatch();
        g_renderer2d->BeginTextBatch();

        g_renderer->SetBlendMode( Renderer::BlendModeAdditive );
        g_renderer2d->StaticSprite( bmpBlur, -180, 100, 360, -200, blurColour );

        g_renderer2d->Line( -540, 100, 1080, 100, blurColour, lineWidth );
        g_renderer2d->Line( -540, -100, 1080, -100, blurColour, lineWidth );
           
        g_renderer->SetBlendMode( Renderer::BlendModeNormal );

        bool showCoastlines = g_preferences->GetInt( PREFS_GRAPHICS_COASTLINES );
        bool showBorders    = g_preferences->GetInt( PREFS_GRAPHICS_BORDERS );

        if( showBorders ) RenderBorders();
        if( showCoastlines ) RenderCoastlines();

        RenderCountryControl();
        RenderWorldMessages();

        g_renderer2d->EndStaticSpriteBatch();
        g_renderer2d->EndLineBatch();
        g_renderer2d->EndTextBatch();
		
        left += GetLongitudeMod();
        right += GetLongitudeMod();
    }

    //
    // Now go through and render objects on top of the landscape.

    GetWindowBounds( &left, &right, &top, &bottom );
    for( int x = 0; x < 2; ++x )
    {
        m_seamIteration = x;  // Track which iteration we're in for seam wrapping
        g_renderer2d->Set2DViewport( left, right, bottom, top, m_pixelX, m_pixelY, m_pixelW, m_pixelH );

        //
        // master scene batching, wrap the entire map rendering loop inside the buffers

        g_renderer2d->BeginStaticSpriteBatch();
        g_renderer2d->BeginRectFillBatch();
        g_renderer2d->BeginRectBatch();
        g_renderer2d->BeginRotatingSpriteBatch();
        g_renderer2d->BeginLineBatch();

        if( g_app->GetWorldRenderer()->m_showPopulation )          
        { 
            RenderPopulationDensity();
        }
        if( g_app->GetWorldRenderer()->m_showNukeUnits )           
        {
            RenderNukeUnits();
        }

        RenderObjects();
        RenderCities();

        g_renderer2d->BeginCircleFillBatch();
        g_renderer2d->BeginCircleBatch();
        
        RenderNodes();

        g_renderer2d->EndCircleFillBatch();
        g_renderer2d->EndCircleBatch();
        g_renderer2d->EndTextBatch();

        RenderGunfire();           
        RenderBlips();        
		RenderSanta();

        g_renderer->SetBlendMode( Renderer::BlendModeAdditive );

        RenderExplosions();
        RenderImpactSymbolsOverlay();
        RenderAnimations();

        if( g_app->GetWorldRenderer()->m_showRadiation ) 
        {
            RenderRadiation();
        }

        if( m_highlightUnit != -1 )
        {
            RenderUnitHighlight( m_highlightUnit );
        }

        RenderWhiteBoard();

        //
        // master scene batching, end all batching systems and flush

        g_renderer2d->EndRectFillBatch();
        g_renderer2d->EndRectBatch();
        g_renderer2d->EndStaticSpriteBatch();
        g_renderer2d->EndLineBatch();
        g_renderer2d->EndRotatingSpriteBatch();

        //
        // render radar outside of the main scene to
        // render it in front of everything, the radar 
        // system batches internally anyway

        if( g_app->GetWorldRenderer()->m_showRadar )               
        {
            RenderRadar();
        }

        left += GetLongitudeMod();
        right += GetLongitudeMod();
    }    

    //
    // render mouse seperately to prevent object info boxes being covered by territory areas

    GetWindowBounds( &left, &right, &top, &bottom );
    for( int x = 0; x < 2; ++x )
    {
        m_seamIteration = x;  // Track which iteration we're in for seam wrapping
        g_renderer2d->Set2DViewport( left, right, bottom, top, m_pixelX, m_pixelY, m_pixelW, m_pixelH );
        
        //
        // begin batching for cursor targets and mouse UI
        // this fixes the blend issue with the cursor target sprite

        g_renderer2d->BeginTextBatch();
        g_renderer2d->BeginStaticSpriteBatch();
        g_renderer2d->BeginLineBatch();
        g_renderer2d->BeginCircleFillBatch();
        g_renderer2d->BeginCircleBatch();
        g_renderer2d->BeginRectBatch();
        g_renderer2d->BeginRectFillBatch();
        g_renderer2d->BeginRotatingSpriteBatch();
        
        if( IsMouseInMapRenderer() )
        {
            RenderDragSelectionMarquee();
            RenderMouse();
        }
        else
        {
            EclWindow *window = EclGetWindow();
            if( stricmp( window->m_name, "Placement" ) == 0 )
            {
                RenderPlacementDetails();
            }
        }
        
        //
        // end background fill before setting blend mode

        g_renderer2d->EndRectFillBatch(); 
        
        g_renderer->SetBlendMode( Renderer::BlendModeAdditive );  // ensure correct blend mode for cursor targets
        g_renderer2d->EndCircleBatch();
        g_renderer2d->EndCircleFillBatch();
        g_renderer2d->EndStaticSpriteBatch();
        g_renderer2d->EndRotatingSpriteBatch();
        g_renderer2d->EndLineBatch();
        g_renderer2d->EndRectBatch();
        g_renderer2d->EndTextBatch();

        g_renderer->SetBlendMode( Renderer::BlendModeNormal );    // reset blend mode after flush
        
        left += GetLongitudeMod();
        right += GetLongitudeMod();
    }

    END_PROFILE( "MapRenderer" );
}


void MapRenderer::RenderPlacementDetails()
{
    float mouseX;
    float mouseY;    
    ConvertPixelsToAngle( g_inputManager->m_mouseX, g_inputManager->m_mouseY, &mouseX, &mouseY );

    int tooltipsEnabled = g_preferences->GetInt( PREFS_INTERFACE_TOOLTIPS );
    if( tooltipsEnabled &&
        m_tooltip &&
        m_tooltip->Size() )
    {
        float titleSize, textSize, gapSize;
        GetStatePositionSizes( &titleSize, &textSize, &gapSize );

        float boxH = (textSize+gapSize);
        float boxW = 18.0f * m_drawScale;
        float boxX = mouseX - (boxW/3);
        float boxY = mouseY - titleSize - titleSize - titleSize*0.5f;        
        float boxSep = gapSize;


        //
        // Fill box

        Colour windowColPrimary   = g_styleTable->GetPrimaryColour( STYLE_POPUP_BACKGROUND );
        Colour windowColSecondary = g_styleTable->GetSecondaryColour( STYLE_POPUP_BACKGROUND );
        Colour windowBorder       = g_styleTable->GetPrimaryColour( STYLE_POPUP_BORDER );    
        Colour fontCol            = g_styleTable->GetPrimaryColour( FONTSTYLE_POPUP_TITLE );
        bool windowOrientation    = g_styleTable->GetStyle( STYLE_POPUP_BACKGROUND )->m_horizontal;

        float alpha = (m_mouseIdleTime-1.0f);
        Clamp( alpha, 0.0f, 1.0f );

        windowColPrimary.m_a    *= alpha;
        windowColSecondary.m_a  *= alpha;
        windowBorder.m_a        *= alpha;
        fontCol.m_a             *= alpha;

        boxY += boxH;
        float totalBoxH = 0.0f;
        totalBoxH += m_tooltip->Size() * -boxH * 0.6f;


        g_renderer->SetBlendMode( Renderer::BlendModeNormal );
        g_renderer2d->RectFill( boxX, boxY, boxW, totalBoxH, windowColPrimary, windowColSecondary, windowOrientation );
        g_renderer2d->Rect( boxX, boxY, boxW, totalBoxH, windowBorder );


        g_renderer->SetFont( "kremlin", true );

        float tooltipY = boxY - 0.5f * m_drawScale;
        RenderTooltip( &boxX, &tooltipY, &boxW, &boxH, &boxSep, alpha );

		g_renderer->SetFont();

        m_tooltip->Empty();
    }    
}


void MapRenderer::RenderExplosions()
{
    START_PROFILE( "Explosions" );
    int myTeamId = g_app->GetWorld()->m_myTeamId;
    
    for( int i = 0; i < g_app->GetWorld()->m_explosions.Size(); ++i )
    {
        if( g_app->GetWorld()->m_explosions.ValidIndex(i) )
        {
            Explosion *explosion = g_app->GetWorld()->m_explosions[i];
            if( IsOnScreen( explosion->m_longitude.DoubleValue(), explosion->m_latitude.DoubleValue() ) )
            {
                if( myTeamId == -1 ||
                    explosion->m_visible[myTeamId] || 
                    g_app->GetWorldRenderer()->CanRenderEverything() )
                {
                    g_app->GetWorld()->m_explosions[i]->Render2D();
                }
            }
        }
    }
    
    
    END_PROFILE( "Explosions" );
}

void MapRenderer::RenderImpactSymbolsOverlay()
{
    START_PROFILE( "ImpactSymbolsOverlay" );
    int myTeamId = g_app->GetWorld()->m_myTeamId;
    for( int i = 0; i < g_app->GetWorld()->m_explosions.Size(); ++i )
    {
        if( g_app->GetWorld()->m_explosions.ValidIndex(i) )
        {
            Explosion *explosion = g_app->GetWorld()->m_explosions[i];
            if( IsOnScreen( explosion->m_longitude.DoubleValue(), explosion->m_latitude.DoubleValue() ) )
            {
                if( myTeamId == -1 ||
                    explosion->m_visible[myTeamId] ||
                    g_app->GetWorldRenderer()->CanRenderEverything() )
                {
                    explosion->RenderImpactSymbol();
                }
            }
        }
    }
    END_PROFILE( "ImpactSymbolsOverlay" );
}

// Symbology: launchsymbol=launch notifications, impactsymbol=nuclear explosions, nukesymbol=targets.
// Future: cruise missile / LACM will use separate symbology (e.g. cruise missilesymbol).
void MapRenderer::RenderNukeImpactZone( Nuke *nuke, float targetLong, float targetLat, float viewerLong, float viewerLat )
{
    float launchLong = nuke->m_launchposition_x.DoubleValue();
    float launchLat = nuke->m_launchposition_y.DoubleValue();
    int curveDirection = nuke->m_curveDirection.IntValue();

    float calculatedAngle = atan2(launchLong - targetLong, launchLat - targetLat);
    if( curveDirection < 0 )
        calculatedAngle += (float)M_PI / 2.0f;

    float distance = g_app->GetWorld()->GetDistance(
        Fixed::FromDouble(viewerLong),
        Fixed::FromDouble(viewerLat),
        Fixed::FromDouble(targetLong),
        Fixed::FromDouble(targetLat)
    ).DoubleValue();

    float sizeFactor = (2.0f * powf(2.71828f, 0.02f * distance)) - 2.0f;
    if( sizeFactor < 0.1f ) sizeFactor = 0.1f;
    if( sizeFactor > 50.0f ) sizeFactor = 50.0f;

    float zoneWidth = nuke->m_impactzone_w * 2.0f * sizeFactor;
    float zoneHeight = nuke->m_impactzone_h * 2.0f * sizeFactor;

    float offsetX = nuke->m_impactzone_x * sizeFactor * 2.0f / 100.0f * nuke->m_cointoss_x + 1.0f;
    float offsetY = nuke->m_impactzone_y * sizeFactor * 2.0f / 100.0f * nuke->m_cointoss_y + 1.0f;

    Colour zoneColour(255, 255, 255, 100);
    Image *impactZoneImage = g_resource->GetImage("graphics/impact_zone.bmp");
    if( impactZoneImage )
    {
        g_renderer2d->RotatingSprite( impactZoneImage, targetLong * offsetX, targetLat * offsetY,
                                     2.0f * zoneWidth, 2.0f * zoneHeight, zoneColour, calculatedAngle );
    }
}

void MapRenderer::RenderAnimations()
{
    
    for( int i = 0; i <  g_app->GetWorldRenderer()->GetAnimations().Size(); ++i )
    {
        if(  g_app->GetWorldRenderer()->GetAnimations().ValidIndex(i) )
        {
            AnimatedIcon *anim =  g_app->GetWorldRenderer()->GetAnimations()[i];
            anim->Update();
            anim->Render2D();
            
            if( anim->IsFinished() )
            {
                 g_app->GetWorldRenderer()->GetAnimations().RemoveData(i);
                delete anim;
            }
        }
    }
    
}


void MapRenderer::RenderRadiation()
{
    Image *boom = g_resource->GetImage( "graphics/population.bmp" );
    for( int i = 0; i < g_app->GetWorld()->m_radiation.Size(); ++i )
    {
        Colour col = Colour( 40+sinf(g_gameTime+i*.14)*30,
                            100+sinf(g_gameTime+i*1.2)*30,
                            40+cosf(g_gameTime+i*1.5)*30,
                            20+sinf(g_gameTime+i*1.1)*5 );
        Vector3<Fixed> *pos = (Vector3<Fixed> *)g_app->GetWorld()->m_radiation[i];
        g_renderer2d->StaticSprite( boom, pos->x.DoubleValue(), pos->y.DoubleValue(), 15.0f, 15.0f, col );
    }
}


void MapRenderer::RenderCountryControl()
{
    START_PROFILE( "Country Control" );
    
    g_renderer->SetBlendMode( Renderer::BlendModeAdditive );

    float worldHeight = 200;
    float topY = worldHeight / 2.0f;

    //
    // Render Water

    if( g_preferences->GetInt( PREFS_GRAPHICS_WATER ) > 0 )
    {
        float mapColourFader = 1.0f;
        Team *myTeam = g_app->GetWorld()->GetMyTeam();
        if( myTeam ) mapColourFader = myTeam->m_teamColourFader;

        Colour waterColour = g_styleTable->GetPrimaryColour( STYLE_WORLD_WATER );
        Colour desaturatedColour = g_styleTable->GetSecondaryColour( STYLE_WORLD_WATER );
        waterColour = waterColour * mapColourFader + desaturatedColour * (1-mapColourFader);

        bmpWater = g_resource->GetImage( "graphics/water.bmp" );
        if( g_preferences->GetInt( PREFS_GRAPHICS_WATER ) == 2 )
        {
            bmpWater = g_resource->GetImage( "graphics/water_shaded.bmp" );
        }

        g_renderer2d->StaticSprite( bmpWater, -180, topY, 360, -worldHeight, waterColour );
    }


    //
    // Render each teams country

    for( int i = 0; i < g_app->GetWorld()->m_teams.Size(); ++i )
    {
        Team *team = g_app->GetWorld()->m_teams[i];
        bool showTeam = m_showTeam[team->m_teamId] && g_app->GetWorld()->GetDefcon() > 3;

        if( showTeam || m_showAllTeams )
        {
            Colour col = team->GetTeamColour();
            col.m_a = 120;
            for( int j = 0; j < team->m_territories.Size(); ++j )
            {
                Image *img = g_app->GetWorldRenderer()->GetTerritoryImage( team->m_territories[j] );
                if( !img ) continue;
                g_renderer->SetBlendMode( Renderer::BlendModeAdditive );
                g_renderer2d->StaticSprite( img, -180, topY, 360, -worldHeight, col );

                if( m_showAllTeams )
                {
                    Vector3<float> populationCentre;
					populationCentre = g_app->GetWorld()->m_populationCenter[team->m_territories[j]];

                    char teamName[256];
                    sprintf( teamName, "%s", team->m_name );
                    strupr(teamName);

                    g_renderer->SetFont( "kremlin", true );
                    g_renderer2d->TextCentreSimple( populationCentre.x, populationCentre.y, White, 7, teamName );
                    g_renderer->SetFont();
                }              
            }


            //
            // Render all our objects to the depth buffer first
            
            if( g_app->GetWorld()->GetDefcon() > 3 &&
                team->m_teamId == g_app->GetWorld()->m_myTeamId )
            {
                g_renderer->SetBlendMode( Renderer::BlendModeNormal );
                float maxDistance = 2.0f / g_app->GetWorld()->GetGameScale().DoubleValue();

                g_renderer->SetDepthBuffer( true, true );
                for( int i = 0; i < g_app->GetWorld()->m_objects.Size(); ++i )
                {
                    if( g_app->GetWorld()->m_objects.ValidIndex(i) )
                    {
                        WorldObject *wobj = g_app->GetWorld()->m_objects[i];
                        if( wobj && 
                            wobj->m_teamId == team->m_teamId &&
                            !wobj->IsMovingObject() )
                        {
                            g_renderer2d->CircleFill( wobj->m_longitude.DoubleValue(), wobj->m_latitude.DoubleValue(),
													maxDistance, 40, Colour(0,0,0,30), true );
                        }
                    }
                }
                g_renderer->SetDepthBuffer( false, false );
            }
        }
    }

    END_PROFILE( "Country Control" );
}

void MapRenderer::RenderGunfire()
{
    START_PROFILE( "Gunfire" );

    int myTeamId = g_app->GetWorld()->m_myTeamId;


    for( int i = 0; i < g_app->GetWorld()->m_gunfire.Size(); ++i )
    {
        if( g_app->GetWorld()->m_gunfire.ValidIndex(i) )
        {
            GunFire *gunFire = g_app->GetWorld()->m_gunfire[i];
            if( IsOnScreen( gunFire->m_longitude.DoubleValue(), gunFire->m_latitude.DoubleValue() ) &&
                (gunFire->m_teamId == myTeamId ||
                 myTeamId == -1 ||
                 gunFire->m_visible[myTeamId] ||
                 g_app->GetWorldRenderer()->CanRenderEverything() ) )
            {
                g_app->GetWorld()->m_gunfire[i]->Render2D();
            }
        }
    }


    END_PROFILE( "Gunfire" );
}

void MapRenderer::RenderWorldMessages()
{
    g_renderer->SetFont( "kremlin", true );

    for( int i = 0; i < g_app->GetWorld()->m_messages.Size(); ++i )
    {
        WorldMessage *msg = g_app->GetWorld()->m_messages[i];
        Colour col(100,100,100,155);
        if( g_app->GetWorld()->GetTeam( msg->m_teamId ) )
        {
            col = g_app->GetWorld()->GetTeam( msg->m_teamId )->GetTeamColour();
        }

        float predictedStateTimer = msg->m_timer.DoubleValue() - g_predictionTime;
        float fractionDone = (3.0f - predictedStateTimer) / 0.5f;
        char caption[512];
        strcpy( caption, msg->m_message );
        if( fractionDone < 1.0f && !msg->m_renderFull )
        {
            int clipIndex = int( strlen(caption) * fractionDone - 1);
            if( clipIndex < 0 ) clipIndex = 0;
            caption[ clipIndex ] = '\x0';
        }

        float longitude = msg->m_longitude.DoubleValue();
        float latitude = msg->m_latitude.DoubleValue();    
        float size = 3.0f;           
        float textWidth = g_renderer2d->TextWidth( msg->m_message, size );
       
        g_renderer2d->TextSimple( longitude - (textWidth/2), latitude+3.0f, White, size, caption );       
        
        if( msg->m_messageType == WorldMessage::TypeLaunch )
        {
            g_renderer2d->Circle( longitude, latitude, size, 30, col, 2.0f );
        }
    }

    g_renderer->SetFont();
}

void MapRenderer::RenderWorldObjectDetails( WorldObject *wobj )
{
    float titleSize, textSize, gapSize;
    GetStatePositionSizes( &titleSize, &textSize, &gapSize );

    //
    // Draw basic box with our allegiance and object type
    
    int numStates = wobj->m_states.Size();

    ////
    //// Render the title bar

    float boxX, boxY, boxW, boxH;
    GetWorldObjectStatePosition( wobj, -2, &boxX, &boxY, &boxW, &boxH );


    char *titleFont = g_styleTable->GetStyle( FONTSTYLE_POPUP_TITLE )->m_fontName;
    
    g_renderer->SetFont( titleFont, true );

    boxY -= boxH * 1.0f;


    //
    // Render more details
        
    if( wobj->IsCityClass() )
    {
        RenderCityObjectDetails( wobj, &boxX, &boxY, &boxW, &boxH, &gapSize );
    }
    else if( wobj->m_teamId == g_app->GetWorld()->m_myTeamId && 
             numStates > 0 )
    {
        if( m_stateRenderTime <= 0.0f )
        {
            RenderFriendlyObjectDetails( wobj, &boxX, &boxY, &boxW, &boxH, &gapSize );
        }
        else
        {
            RenderFriendlyObjectMenu( wobj, &boxX, &boxY, &boxW, &boxH, &gapSize );
        }
    }
    else if( wobj->m_teamId != g_app->GetWorld()->m_myTeamId )
    {
        RenderEnemyObjectDetails( wobj, &boxX, &boxY, &boxW, &boxH, &gapSize );
    }

    g_renderer->SetFont();
}


void MapRenderer::RenderCityObjectDetails( WorldObject *wobj, float *boxX, float *boxY, float *boxW, float *boxH, float *boxSep )
{
    City *city = (City *) wobj;

	float originalAlpha = g_renderer2d->m_alpha * (1.0f - ( m_highlightTime / 1.0f ));
    int alpha = originalAlpha * 255;

    float titleSize, textSize, gapSize;
    GetStatePositionSizes( &titleSize, &textSize, &gapSize );
	
    float totalBoxH = 1.6 * -(*boxH);
    if( city->m_dead > 0 )
    {
        totalBoxH += 0.4f * -(*boxH);
    }

    *boxW -= 5 * m_drawScale;

    int tooltipsEnabled = g_preferences->GetInt( PREFS_INTERFACE_TOOLTIPS );
    if( tooltipsEnabled && m_tooltip )
    {
        totalBoxH += m_tooltip->Size() * -(*boxH) * 0.6f;
    }

    Colour windowColPrimary   = g_styleTable->GetPrimaryColour( STYLE_POPUP_BACKGROUND );
    Colour windowColSecondary = g_styleTable->GetSecondaryColour( STYLE_POPUP_BACKGROUND );
    Colour windowBorder       = g_styleTable->GetPrimaryColour( STYLE_POPUP_BORDER );    
    Colour fontCol            = g_styleTable->GetPrimaryColour( FONTSTYLE_POPUP );

    bool windowOrientation    = g_styleTable->GetStyle( STYLE_POPUP_BACKGROUND )->m_horizontal;

    windowColPrimary    *= originalAlpha;
    windowColSecondary  *= originalAlpha;
    windowBorder        *= originalAlpha;
    fontCol             *= originalAlpha;

    *boxY += *boxH;

    g_renderer2d->RectFill( *boxX, *boxY, *boxW, totalBoxH, windowColPrimary, windowColSecondary, windowOrientation );
    g_renderer2d->Rect( *boxX, *boxY, *boxW, totalBoxH, windowBorder );
    float timeSize = 1.5f * m_drawScale;


    //
    // Render title

    *boxY -= *boxH;

    char *titleFont = g_styleTable->GetStyle( FONTSTYLE_POPUP )->m_fontName;
    g_renderer->SetFont( titleFont, true );

    const char *objName = WorldObject::GetName( wobj->m_type );
    Team *team = g_app->GetWorld()->GetTeam(wobj->m_teamId);
    const char *teamName = team ? team->GetTeamName() : "";

    char caption[256];
    sprintf( caption, "%s  %s", teamName, objName );
    strupr( caption );

    g_renderer2d->Text( *boxX + gapSize, 
        *boxY + (*boxH-textSize)/2.0f, 
        fontCol, 
        textSize, 
        caption );

    
    //
    // 1. Population

    *boxY -= *boxH * 0.5f;

    float textYPos = *boxY + (*boxH - textSize)/2;

	char number[32];
    strcpy( caption, LANGUAGEPHRASE("dialog_mapr_city_population") );
	sprintf( number, "%2.1f", city->m_population / 1000000.0f );
	LPREPLACESTRINGFLAG( 'P', number, caption );
    g_renderer2d->TextSimple( *boxX + *boxSep, textYPos, fontCol, textSize*0.6f, caption );

    *boxY -= *boxH * 0.5f;

    //
    // 2. Num Dead

    if( city->m_dead > 0 )
    {
        float textYPos = *boxY + (*boxH - textSize)/2;

		char caption[128];
		char number[32];
        strcpy( caption, LANGUAGEPHRASE("dialog_mapr_city_dead") );
		sprintf( number, "%2.1f", city->m_dead / 1000000.0f );
		LPREPLACESTRINGFLAG( 'D', number, caption );
        g_renderer2d->TextSimple( *boxX + *boxSep, textYPos, fontCol, textSize*0.6f, caption );

        *boxY -= *boxH * 0.4f;
    }
    

    //
    // 3. Tooltips

    if( tooltipsEnabled && m_tooltip )
    {
        *boxY += *boxH * 0.4f;
        
        RenderTooltip( boxX, boxY, boxW, boxH, boxSep, originalAlpha );
    }

	g_renderer->SetFont();
}


void MapRenderer::RenderTooltip( float *boxX, float *boxY, float *boxW, float *boxH, float *boxSep, float alpha )
{
    if( m_tooltip )
    {
        float titleSize, textSize, gapSize;
        GetStatePositionSizes( &titleSize, &textSize, &gapSize );

        Colour fontCol = g_styleTable->GetPrimaryColour( FONTSTYLE_POPUP );
        fontCol.m_a *= alpha;


        for( int i = 0; i < m_tooltip->Size(); ++i )
        {
            const char *thisLine = m_tooltip->GetData(i);
            *boxY -= *boxH * 0.4f;
            if( i > 0 ) *boxY -= *boxH * 0.2f;

			const char *phraseLMB = LANGUAGEPHRASE("tooltip_lmb");
			const char *phraseRMB = LANGUAGEPHRASE("tooltip_rmb");

            Image *img = NULL;
			if( phraseLMB && strncmp( thisLine, phraseLMB, strlen( phraseLMB ) ) == 0 )
			{
				img = g_resource->GetImage( "gui/lmb.bmp" );
			}
			else if( phraseRMB && strncmp( thisLine, phraseRMB, strlen( phraseRMB ) ) == 0 )
			{
				img = g_resource->GetImage( "gui/rmb.bmp" );
			}

            if( img )
            {
                float iconSize = textSize * 0.9f;
                float iconY = *boxY + iconSize - 0.3f * m_drawScale ;

                g_renderer->SetBlendMode( Renderer::BlendModeAdditive );
                g_renderer2d->StaticSprite( img, *boxX + *boxSep, iconY, iconSize, -iconSize, Colour(255,255,255,255*alpha) );
                thisLine += 5;

                g_renderer2d->TextSimple( *boxX + *boxSep + 2 * m_drawScale, *boxY, fontCol, textSize*0.6f, thisLine );
            }
            else
            {
                g_renderer2d->TextSimple( *boxX + *boxSep, *boxY, fontCol, textSize*0.6f, thisLine );
            }
        }
    }
}


void MapRenderer::RenderFriendlyObjectDetails( WorldObject *wobj, float *boxX, float *boxY, float *boxW, float *boxH, float *boxSep )
{
    float originalAlpha = g_renderer2d->m_alpha * (1.0f - ( m_highlightTime / 1.0f ));
    originalAlpha = min( originalAlpha, 0.8f );
    int alpha = originalAlpha * 255;

    float titleSize, textSize, gapSize;
    GetStatePositionSizes( &titleSize, &textSize, &gapSize );

    *boxY += *boxH;
    float inset = 0.3f * m_drawScale;

    Colour windowColPrimary   = g_styleTable->GetPrimaryColour( STYLE_POPUP_BACKGROUND );
    Colour windowColSecondary = g_styleTable->GetSecondaryColour( STYLE_POPUP_BACKGROUND );
    Colour windowBorder       = g_styleTable->GetPrimaryColour( STYLE_POPUP_BORDER );    
    Colour fontCol            = g_styleTable->GetPrimaryColour( FONTSTYLE_POPUP );

    bool windowOrientation    = g_styleTable->GetStyle( STYLE_POPUP_BACKGROUND )->m_horizontal;

    if( wobj->m_stateTimer <= Fixed::Hundredths(10) )
    {
        *boxW -= 7 * m_drawScale;
    }

    windowColPrimary.m_a    *= originalAlpha;
    windowColSecondary.m_a  *= originalAlpha;
    windowBorder.m_a        *= originalAlpha;
    fontCol.m_a             *= originalAlpha;

    //
    // Spawn object counters (fighters, bombers, ammunition)
    // DESIGN: Use tooltip counters for ammunition instead of on-sprite graphics (e.g. smallnuke).
    // When adding new units/buildings with ammo, add a case here and avoid per-unit sprite overlays.

    int numFighters = -1;
    int numBombers = -1;
    int numNukes = -1;

    switch( wobj->m_type )
    {
        case WorldObject::TypeSilo:
            //numNukes = wobj->m_states[1]->m_numTimesPermitted;
            break;

        case WorldObject::TypeAirBase:
        case WorldObject::TypeCarrier:
            numFighters = wobj->m_states[0]->m_numTimesPermitted;
            numBombers = wobj->m_states[1]->m_numTimesPermitted;
            // No nukes - airbase/carrier have infinite resupply, don't show nuke storage
            break;

        case WorldObject::TypeBomber:
            numNukes = wobj->m_states[1]->m_numTimesPermitted;
            break;

        case WorldObject::TypeSub:
            numNukes = wobj->m_states[3]->m_numTimesPermitted;
            break;
    }


    float totalBoxH = 1 * -(*boxH);
    if( numFighters != -1 || numBombers != -1 || numNukes != -1 )
    {
        totalBoxH += 0.9f * -(*boxH);
    }

    int tooltipsEnabled = g_preferences->GetInt( PREFS_INTERFACE_TOOLTIPS );
    if( tooltipsEnabled && m_tooltip )
    {
        totalBoxH += m_tooltip->Size() * -(*boxH) * 0.6f;
    }

    //
    // Render main box

    g_renderer2d->RectFill( *boxX, *boxY, *boxW, totalBoxH, windowColPrimary, windowColSecondary, windowOrientation );
    g_renderer2d->Rect( *boxX, *boxY, *boxW, totalBoxH, windowBorder );


    *boxY -= *boxH;

    //
    // Render current state details

    WorldObjectState *state = wobj->m_states[wobj->m_currentState];

    char stateName[256];
    strcpy( stateName, LANGUAGEPHRASE("dialog_mapr_mode") );
	LPREPLACESTRINGFLAG( 'M', state->GetStateName(), stateName );

    float textYPos = *boxY + (*boxH - textSize)/2;
    g_renderer2d->TextSimple( *boxX + *boxSep, textYPos, fontCol, textSize, stateName );
        
    if( wobj->m_stateTimer > 0 && 
        state->m_numTimesPermitted != 0 )
    {
        char stimeTodo[64];
        strcpy( stimeTodo, LANGUAGEPHRASE("dialog_mapr_secs") );
		LPREPLACEINTEGERFLAG( 'S', wobj->m_stateTimer.IntValue(), stimeTodo );

        g_renderer2d->TextRight( *boxX + *boxSep + *boxW - *boxSep - (1.0f * m_drawScale), 
                               textYPos, fontCol, textSize, stimeTodo );
    }


    //
    // tooltip

    if( tooltipsEnabled )
    {
        RenderTooltip( boxX, boxY, boxW, boxH, boxSep, originalAlpha );
    }


    //
    // Spawn object counters

    *boxY -= *boxH;
    float xPos = *boxX;
    float yPos = *boxY + 1.4f * m_drawScale;
    float objSize = 1.0f * m_drawScale;
    float gap = objSize * 2.0f;

    Colour col(255,255,255,alpha);

    float totalSize = 0;
    if( numFighters != -1 ) totalSize += gap * numFighters + gap * 0.5f;
    if( numBombers != -1 ) totalSize += gap * numBombers + gap * 0.5f;
    if( numNukes != -1 ) totalSize += gap * numNukes * 0.5f + gap * 0.25f;

    float maxSize = *boxW;
    if( totalSize > maxSize )
    {
        float fractionMore = totalSize / maxSize;
        gap /= fractionMore;
    }


    if( numFighters == 0 && numBombers == 0 && numNukes == 0 )
    {
        col.m_a = 150;
        g_renderer2d->TextSimple( xPos + gap/2, yPos - gap/2, col, textSize, LANGUAGEPHRASE("dialog_mapr_empty") );
    }
    else
    {
        g_renderer->SetBlendMode( Renderer::BlendModeAdditive );

        //
        // Fighters

        if( numFighters != -1 )
        {
            Image *img = g_resource->GetImage( "graphics/fighter.bmp" );
            for( int i = 0; i < numFighters; ++i )
            {
                g_renderer2d->RotatingSprite( img, xPos+=gap, yPos, objSize, objSize, col, 0 );
            }
            
            if( numFighters > 0 ) xPos += gap*0.5f;
        }


        //
        // Bombers
        
        if( numBombers != -1 )
        {
            Image *img = g_resource->GetImage( "graphics/bomber.bmp" );
            Image *nuke = g_resource->GetImage( "graphics/nuke.bmp" );
            for( int i = 0; i < numBombers; ++i )
            {
                g_renderer2d->RotatingSprite( img, xPos+=gap, yPos, objSize*1.2f, objSize*1.2f, col, 0 );
                if( (i+1) <= numNukes )
                {
                    g_renderer2d->RotatingSprite( nuke, xPos, yPos, objSize*0.65f, objSize*0.65f, col, 0 );
                }
            }
            if( numBombers > 0 ) xPos += gap*0.5f;
        }


        //
        // Nukes

        if( numNukes != -1 )
        {
            if( numBombers != -1 ) numNukes -= numBombers;
            Image *img = g_resource->GetImage( "graphics/nuke.bmp" );
            for( int i = 0; i < numNukes; ++i )
            {
                g_renderer2d->RotatingSprite( img, xPos+=gap*0.5f, yPos, objSize, objSize, col, 0 );
            }
        }
    }
}


void MapRenderer::RenderFriendlyObjectMenu( WorldObject *wobj, float *boxX, float *boxY, float *boxW, float *boxH, float *boxSep )
{
#ifdef NON_PLAYABLE
    RenderEnemyObjectDetails( wobj, boxX, boxY, boxW, boxH, boxSep );
    return;
#endif

	int numStates = wobj->m_states.Size();
    float originalAlpha = g_renderer2d->m_alpha * (1.0f - ( m_highlightTime / 1.0f ));
    int alpha = originalAlpha * 255;

    if( m_stateRenderTime <= 0.0f )
    {
        alpha *= 0.5f;
    }

    float titleSize, textSize, gapSize;
    GetStatePositionSizes( &titleSize, &textSize, &gapSize );



	*boxY += *boxH;
    float totalBoxH = numStates * -(*boxH);

    float inset = 0.3f * m_drawScale;

    bool actionQueue = wobj->IsActionQueueable() &&
                       wobj->m_actionQueue.Size() > 0;

    if( actionQueue )
    {
        totalBoxH -= *boxH;
    }

    Colour windowColPrimary   = g_styleTable->GetPrimaryColour( STYLE_POPUP_BACKGROUND );
    Colour windowColSecondary = g_styleTable->GetSecondaryColour( STYLE_POPUP_BACKGROUND );
    Colour windowBorder       = g_styleTable->GetPrimaryColour( STYLE_POPUP_BORDER );    
    Colour highlightPrimary   = g_styleTable->GetPrimaryColour( STYLE_POPUP_HIGHLIGHT );
    Colour highlightSecondary = g_styleTable->GetSecondaryColour( STYLE_POPUP_HIGHLIGHT );
    Colour selectedPrimary    = g_styleTable->GetPrimaryColour( STYLE_POPUP_SELECTION );
    Colour selectedSecondary  = g_styleTable->GetSecondaryColour( STYLE_POPUP_SELECTION );
    Colour fontCol            = g_styleTable->GetPrimaryColour( FONTSTYLE_POPUP );

    bool windowOrientation    = g_styleTable->GetStyle( STYLE_POPUP_BACKGROUND )->m_horizontal;
    bool highlightOrientation = g_styleTable->GetStyle( STYLE_POPUP_HIGHLIGHT )->m_horizontal;
    bool selectOrientation    = g_styleTable->GetStyle( STYLE_POPUP_BACKGROUND )->m_horizontal;

    windowColPrimary.m_a    *= originalAlpha;
    windowColSecondary.m_a  *= originalAlpha;
    windowBorder.m_a        *= originalAlpha;
    highlightPrimary.m_a    *= originalAlpha;
    highlightSecondary.m_a  *= originalAlpha;
    selectedPrimary.m_a     *= originalAlpha;
    selectedSecondary.m_a   *= originalAlpha;
    fontCol.m_a             *= originalAlpha;


    //
    // Render main box

    g_renderer2d->RectFill( *boxX, *boxY, *boxW, totalBoxH, windowColPrimary, windowColSecondary, windowOrientation );
    g_renderer2d->Rect( *boxX, *boxY, *boxW, totalBoxH, windowBorder );
    



    //
    // Render Clear ActionQueue button

    if( actionQueue )
    {
        float stateX, stateY, stateW, stateH;
        GetWorldObjectStatePosition( wobj, -1, &stateX, &stateY, &stateW, &stateH );

        if( m_currentStateId == CLEARQUEUE_STATEID )
        {
            g_renderer2d->RectFill( stateX+inset, 
                                  stateY+inset, 
                                  stateW-inset*2, 
                                  stateH-inset*2, 
                                  highlightSecondary, 
                                  highlightPrimary, 
                                  highlightOrientation );
        }

        float textYPos = stateY + (stateH - textSize)/2;

        g_renderer2d->TextSimple( stateX + *boxSep, textYPos, fontCol, textSize, LANGUAGEPHRASE("dialog_mapr_cancel_all_orders") );
    }

    //
    // Render our state buttons
    for( int i = 0; i < numStates; ++i )
    {
        WorldObjectState *state = wobj->m_states[i];
        float stateX, stateY, stateW, stateH;
        GetWorldObjectStatePosition( wobj, i, &stateX, &stateY, &stateW, &stateH );
            
        char *caption = NULL;
        float timeTodo = 0.0f;

		if( i == m_currentStateId &&
            state->m_numTimesPermitted != 0 &&
            state->m_defconPermitted >= g_app->GetWorld()->GetDefcon())
        {
            g_renderer2d->RectFill( stateX+inset, 
                                  stateY+inset, 
                                  stateW-inset*2, 
                                  stateH-inset*2, 
                                  highlightSecondary, 
                                  highlightPrimary, 
                                  highlightOrientation );            
        }
        
        if( i == wobj->m_currentState )
        {
            g_renderer2d->RectFill( stateX+inset, 
                                  stateY+inset, 
                                  stateW-inset*2, 
                                  stateH-inset*2, 
                                  selectedSecondary, 
                                  selectedPrimary, 
                                  selectOrientation );
            g_renderer2d->Rect( stateX+inset, 
                              stateY+inset, 
                              stateW-inset*2, 
                              stateH-inset*2,
                              windowBorder );
        }
                        
        if( i == wobj->m_currentState && wobj->m_stateTimer <= 0 )
        {
            caption = state->GetStateName();
            timeTodo = 0.0f;
        }
        else if( i == wobj->m_currentState && wobj->m_stateTimer > 0 )
        {
            caption = state->GetPreparingName();
            timeTodo = wobj->m_stateTimer.DoubleValue();
        }
        else
        {
            caption = state->GetPrepareName();
            timeTodo = state->m_timeToPrepare.DoubleValue();
        }    
        
        if( state->m_numTimesPermitted != -1 )
        {
            sprintf(caption, "%s (%u)", caption, state->m_numTimesPermitted );
        }       

        Colour textCol;
        if( state->m_defconPermitted < g_app->GetWorld()->GetDefcon() ||
            state->m_numTimesPermitted == 0 )
        {
            textCol = Colour( 100, 100, 100, alpha*0.5f );
        }
        else
        {
            textCol = fontCol;
        }

        float textYPos = stateY + (stateH - textSize)/2;
        g_renderer2d->TextSimple( stateX + *boxSep, textYPos, textCol, textSize, caption );

        if( state->m_defconPermitted < g_app->GetWorld()->GetDefcon() )
        {
            char defconAllowed[64];
            strcpy( defconAllowed, LANGUAGEPHRASE("dialog_worldstatus_defcon_x") );
			LPREPLACEINTEGERFLAG( 'D', state->m_defconPermitted, defconAllowed );
            g_renderer2d->TextRight( stateX + stateW - (1.0f * m_drawScale), 
                                   textYPos, textCol, textSize, defconAllowed );
        }
        else if( timeTodo > 0.0f && state->m_numTimesPermitted != 0 )
        {
            char stimeTodo[64];
            strcpy( stimeTodo, LANGUAGEPHRASE("dialog_mapr_secs") );
			LPREPLACEINTEGERFLAG( 'S', int(timeTodo), stimeTodo );
            g_renderer2d->TextRight( stateX + stateW - (1.0f * m_drawScale), 
                                   textYPos, textCol, textSize, stimeTodo );
		}

        //
        // Render problems with the current highlighted state 

        if( i == m_currentStateId )
        {
            if( state->m_numTimesPermitted == 0 )
            {
                g_renderer2d->RectFill( stateX, stateY, stateW, stateH, Colour(0,0,0,100) );
                strcpy( caption, LANGUAGEPHRASE("dialog_mapr_empty") );
                g_renderer2d->TextCentre( stateX + stateW/2, textYPos, Colour(255,0,0,255), textSize, caption );
            }
            else if( state->m_defconPermitted < g_app->GetWorld()->GetDefcon())
            {                
                g_renderer2d->RectFill( stateX, stateY, stateW, stateH, Colour(0,0,0,100) );
                strcpy( caption, LANGUAGEPHRASE("dialog_mapr_not_before_defcon_x") );
				LPREPLACEINTEGERFLAG( 'D', state->m_defconPermitted, caption );
                g_renderer2d->TextCentre( stateX + stateW/2, textYPos, Colour(255,0,0,255), textSize, caption );
            }
        }
    }
}

void MapRenderer::GetWorldObjectStatePosition( WorldObject *wobj, int state, float *screenX, float *screenY, float *screenW, float *screenH )
{
    float titleSize, textSize, gapSize;
    GetStatePositionSizes( &titleSize, &textSize, &gapSize );
 
    float predictedLongitude = ( wobj->m_longitude + wobj->m_vel.x * Fixed::FromDouble(g_predictionTime)
													 * g_app->GetWorld()->GetTimeScaleFactor() ).DoubleValue();
    float predictedLatitude  = ( wobj->m_latitude + wobj->m_vel.y * Fixed::FromDouble(g_predictionTime)
													* g_app->GetWorld()->GetTimeScaleFactor() ).DoubleValue();

    *screenH = (textSize+gapSize);
    *screenW = 27.0f;
    *screenX = predictedLongitude - (*screenW/4 * m_drawScale);

    if( wobj->m_teamId == g_app->GetWorld()->m_myTeamId &&
        !wobj->IsCityClass() )
    {
        *screenW = 34.0f;
    }

    if( state == -1 )
    {
        // Clear Queue state
        int numStates = wobj->m_states.Size();
        *screenY = predictedLatitude - titleSize - (numStates+1) * (*screenH);
    }
    else if( state == -2 )
    {
        // Title bar state
        *screenY = predictedLatitude - titleSize;
        *screenH = titleSize;
    }
    else
    {
        // Ordinary state
        *screenY = predictedLatitude - titleSize - ( state +1 ) * (*screenH);
    }

    *screenW = *screenW * m_drawScale;
}


void MapRenderer::GetStatePositionSizes( float *titleSize, float *textSize, float *gapSize )
{
    *titleSize = 3.0f * m_drawScale;
    *textSize = 1.8f * m_drawScale;
    *gapSize = 1.0f * m_drawScale;
}


void MapRenderer::RenderEnemyObjectDetails( WorldObject *wobj, float *boxX, float *boxY, float *boxW, float *boxH, float *boxSep )
{
	// This is an ENEMY vessel
 
	if( wobj->m_teamId != TEAMID_SPECIALOBJECTS )
	{
        float titleSize, textSize, gapSize;
        GetStatePositionSizes( &titleSize, &textSize, &gapSize );

        float originalAlpha = g_renderer2d->m_alpha * (1.0f - ( m_highlightTime / 1.0f ));
        int alpha = originalAlpha * 255;

        Colour windowColPrimary   = g_styleTable->GetPrimaryColour( STYLE_POPUP_BACKGROUND );
        Colour windowColSecondary = g_styleTable->GetSecondaryColour( STYLE_POPUP_BACKGROUND );
        Colour windowBorder       = g_styleTable->GetPrimaryColour( STYLE_POPUP_BORDER );    
        Colour fontCol            = g_styleTable->GetPrimaryColour( FONTSTYLE_POPUP );

        bool windowOrientation    = g_styleTable->GetStyle( STYLE_POPUP_BACKGROUND )->m_horizontal;

        windowColPrimary.m_a    *= originalAlpha;
        windowColSecondary.m_a  *= originalAlpha;
        windowBorder.m_a        *= originalAlpha;
        fontCol.m_a             *= originalAlpha;

        if( g_app->GetWorldRenderer()->GetCurrentSelectionId() == -1 )
        {
            //
            // Render title

            g_renderer->SetBlendMode( Renderer::BlendModeNormal );
            g_renderer2d->RectFill( *boxX, *boxY, *boxW, *boxH, windowColPrimary, windowColSecondary, windowOrientation );
            g_renderer2d->Rect    ( *boxX, *boxY, *boxW, *boxH, windowBorder );
            
            char *titleFont = g_styleTable->GetStyle( FONTSTYLE_POPUP )->m_fontName;
            g_renderer->SetFont( titleFont, true );

            const char *objName = WorldObject::GetName( wobj->m_type );
            Team *team = g_app->GetWorld()->GetTeam(wobj->m_teamId);
            const char *teamName = team ? team->GetTeamName() : "";

            char caption[256];
            sprintf( caption, "%s  %s", teamName, objName );
            strupr( caption );

            g_renderer2d->Text( *boxX + gapSize, 
                              *boxY + (*boxH-textSize)/2.0f, 
                              fontCol, 
                              textSize, 
                              caption );
        }

        *boxY += *boxH;

        //
        // If we are targeting it then bring up some statistics

        WorldObject *ourObj = g_app->GetWorld()->GetWorldObject( g_app->GetWorldRenderer()->GetCurrentSelectionId() );
		if( ourObj)
		{
			float totalBoxH = 1 * -(*boxH);

            int tooltipsEnabled = g_preferences->GetInt( PREFS_INTERFACE_TOOLTIPS );
            if( tooltipsEnabled && m_tooltip )
            {
                totalBoxH += m_tooltip->Size() * -(*boxH) * 0.6f;
            }

			g_renderer2d->RectFill( *boxX, *boxY, *boxW, totalBoxH, windowColPrimary, windowColSecondary, windowOrientation );
			g_renderer2d->Rect( *boxX, *boxY, *boxW, totalBoxH, windowBorder );
            
			int attackOdds = g_app->GetWorld()->GetAttackOdds( ourObj->m_type, wobj->m_type, ourObj->m_objectId );
				        
			char odds[256];
            Colour col;
            if( attackOdds == 0 ) 
            {
                col = Colour(150,150,150,alpha);
                sprintf( odds, "%s", LANGUAGEPHRASE("odds_zero"));
            }
            else if( attackOdds < 5 ) 
            {
                col = Colour(255,0,0,alpha);
                sprintf( odds, "%s", LANGUAGEPHRASE("odds_vlow") );
            }
            else if( attackOdds < 10 ) 
            {
                col = Colour(255,150,0,alpha );
                sprintf( odds, "%s", LANGUAGEPHRASE("odds_low") );
            }
            else if( attackOdds < 20 ) 
            {
                col = Colour(255,255,0,alpha );
                sprintf( odds, "%s", LANGUAGEPHRASE("odds_med") );
            }
            else if( attackOdds <= 25 )
            {
                col = Colour(150,255,0,alpha);
                sprintf( odds, "%s", LANGUAGEPHRASE("odds_high") );
            }
            else
            {
                col = Colour(0,255,0,alpha);
                sprintf( odds, "%s", LANGUAGEPHRASE("odds_vhigh") );
            }

            strupr(odds);
            
			g_renderer2d->TextSimple( *boxX + *boxSep, *boxY-textSize-(0.5f * m_drawScale), col, textSize, odds );


            //
            // tooltip

            if( tooltipsEnabled && m_tooltip )
            {
                *boxY -= *boxH;

                RenderTooltip( boxX, boxY, boxW, boxH, boxSep, originalAlpha );
            }
		}
    }

	g_renderer->SetFont();
}

void MapRenderer::RenderMouse()
{
    float longitude;
    float latitude;    
    ConvertPixelsToAngle( g_inputManager->m_mouseX, g_inputManager->m_mouseY, &longitude, &latitude );
    
    // render long/lat on mouse

#ifdef _DEBUG
    if( g_keys[KEY_L] )
    {
        char pos[128];
        sprintf( pos, "long %2.1f lat %2.1f", longitude, latitude );
        g_renderer->SetFont( "zerothre", true );
        g_renderer2d->TextSimple( longitude, latitude, White, 3.0f, pos );
        g_renderer->SetFont();
    }
#endif

    g_renderer->SetBlendMode( Renderer::BlendModeNormal );


    if( m_draggingCamera )
    {
        Image *move = g_resource->GetImage( "gui/move.bmp" );
        float screenW = (float)g_windowManager->WindowW();
        float pixelsPerDegree = (screenW / 360.0f) / m_zoomFactor; // divide by zoom to keep screen size constant
        float size = 48.0f / pixelsPerDegree; 
        g_renderer2d->StaticSprite( move, longitude - size/2, latitude - size/2, size, size, White );
    }


    WorldObject *selection = g_app->GetWorld()->GetWorldObject( g_app->GetWorldRenderer()->GetCurrentSelectionId() );
    WorldObject *highlight = g_app->GetWorld()->GetWorldObject( g_app->GetWorldRenderer()->GetCurrentHighlightId() );

    if( selection && selection->m_teamId == TEAMID_SPECIALOBJECTS ) selection = NULL;
    if( highlight && highlight->m_teamId == TEAMID_SPECIALOBJECTS ) highlight = NULL;

    float timeScale = g_app->GetWorld()->GetTimeScaleFactor().DoubleValue();


    //
    // Get new the tooltip ready

    LList<const char *> *tooltip = new LList<const char *>();


    //
    // Render our current selection
#ifndef NON_PLAYABLE
    if( selection )
    {
        float predictedLongitude = selection->m_longitude.DoubleValue() + selection->m_vel.x.DoubleValue() * g_predictionTime * timeScale;
        float predictedLatitude = selection->m_latitude.DoubleValue() + selection->m_vel.y.DoubleValue() * g_predictionTime  * timeScale;
        float size = 3.6f;
        if( GetZoomFactor() <= 0.25f )
        {
            size *= GetZoomFactor() * 4;
        }

        Team *team = g_app->GetWorld()->GetTeam(selection->m_teamId);
        Colour col(128,128,128);
        if( team ) col = team->GetTeamColour();
        col.m_a = 250;


        //
        // Render action cursor from our selection

        float actionCursorLongitude = longitude;
        float actionCursorLatitude = latitude;
        Colour actionCursorCol( 50,50,50,100 );
        float actionCursorSize = 2.0f;
        float actionCursorAngle = 0;
        Colour spawnUnitCol( 100,100,100,100 );
        float spawnUnitSize = 3.0f;

        if( GetZoomFactor() <=0.25f )
        {
            actionCursorSize *= GetZoomFactor() * 4;
            spawnUnitSize *= GetZoomFactor() * 4;
        }

        int validCombatTarget = selection->IsValidCombatTarget( g_app->GetWorldRenderer()->GetCurrentHighlightId() );
        int validMovementTarget = selection->IsValidMovementTarget( Fixed::FromDouble(longitude), Fixed::FromDouble(latitude) );

        bool fleetValidMovement = true;
        Fleet *fleet = team->GetFleet(selection->m_fleetId);
        if( fleet ) fleetValidMovement = fleet->IsValidFleetPosition( Fixed::FromDouble(longitude), Fixed::FromDouble(latitude) );
        
        if( highlight && validCombatTarget > 0 )
        {
            // Highlighted a valid enemy unit
            actionCursorLongitude = (highlight->m_longitude + highlight->m_vel.x 
															  * Fixed::FromDouble(g_predictionTime)
															  * Fixed::FromDouble(timeScale)).DoubleValue();
            actionCursorLatitude = highlight->m_latitude.DoubleValue() + highlight->m_vel.y.DoubleValue() * g_predictionTime  * timeScale;            
            actionCursorCol.Set( 255, 0, 0, 255 );
            actionCursorSize *= 1.5f;
            actionCursorAngle = g_gameTime * -1;

            if( validCombatTarget == WorldObject::TargetTypeLand )
            {
                actionCursorCol.Set( 0, 0, 255, 255 );                
            }
            else if( validCombatTarget > WorldObject::TargetTypeValid )
            {
                spawnUnitCol.Set( 255, 0, 0, 255 );
            }
        }
        else if( fleet )
        {
            if( fleetValidMovement )
            {
                actionCursorCol.Set( 0, 0, 255, 255 );
                actionCursorSize *= 1.5f;
                actionCursorAngle = g_gameTime * -0.5f;
            }

            if( validMovementTarget > WorldObject::TargetTypeValid )
            {
                spawnUnitCol.Set( 0, 0, 255, 255 );
            }
        }
        else if( validMovementTarget > 0 )
        {
            // Highlighted a valid movement target
            actionCursorCol.Set( 0, 0, 255, 255 );
            actionCursorSize *= 1.5f;
            actionCursorAngle = g_gameTime * -0.5f;

            if( validMovementTarget > WorldObject::TargetTypeValid )
            {
                spawnUnitCol.Set( 0, 0, 255, 255 );
            }

            // check for landing
            if( highlight && 
                highlight->IsAircraftLauncher() &&
                selection->IsAircraft() )
            {   
                actionCursorLongitude = highlight->m_longitude.DoubleValue() + highlight->m_vel.x.DoubleValue() * g_predictionTime * timeScale;
                actionCursorLatitude = highlight->m_latitude.DoubleValue() + highlight->m_vel.y.DoubleValue() * g_predictionTime  * timeScale;
            }
        }

        g_renderer->SetBlendMode( Renderer::BlendModeAdditive );

        Image *img = g_resource->GetImage( "graphics/cursor_target.bmp" );
        g_renderer2d->RotatingSprite( img, actionCursorLongitude, actionCursorLatitude, 
                                 actionCursorSize, actionCursorSize, 
                                 actionCursorCol, actionCursorAngle );

        //
        // Render problem messages with target

        int validTarget = validCombatTarget;
        if( validTarget <= WorldObject::TargetTypeInvalid ) validTarget = validMovementTarget;

        if( validTarget < WorldObject::TargetTypeInvalid )
        {
            g_renderer->SetFont( "kremlin", true );
            switch( validTarget )
            {
                case WorldObject::TargetTypeDefconRequired:
                {
                    int defconRequired = selection->m_states[selection->m_currentState]->m_defconPermitted;
					char caption[128];
                    strcpy( caption, LANGUAGEPHRASE("dialog_mapr_defcon_x_required") );
					LPREPLACEINTEGERFLAG( 'D', defconRequired, caption );
                    g_renderer2d->TextCentreSimple( actionCursorLongitude, actionCursorLatitude+actionCursorSize, White, 2, caption );
                    break;
                }

                case WorldObject::TargetTypeOutOfRange:
                    g_renderer2d->TextCentreSimple( actionCursorLongitude, actionCursorLatitude+actionCursorSize, White, 2, LANGUAGEPHRASE("dialog_mapr_out_of_range") );
                    break;

                case WorldObject::TargetTypeOutOfStock:
                    g_renderer2d->TextCentreSimple( actionCursorLongitude, actionCursorLatitude+actionCursorSize, White, 2, LANGUAGEPHRASE("dialog_mapr_empty") );
                    break;
            }
            g_renderer->SetFont();
        }


        //
        // We can possibly spawn a unit to this location
        
        int spawnType = validCombatTarget;
        if( spawnType <= WorldObject::TargetTypeInvalid ) spawnType = validMovementTarget;    
        if( spawnType > WorldObject::TargetTypeValid)
        {
            switch( spawnType )
            {
                case WorldObject::TargetTypeLaunchFighter:      
                    img = g_resource->GetImage( "graphics/fighter.bmp" );       
                    tooltip->PutData( LANGUAGEPHRASE("tooltip_spawnfighter") );
                    break;

                case WorldObject::TargetTypeLaunchBomber:       
                    img = g_resource->GetImage( "graphics/bomber.bmp" );        
                    tooltip->PutData( LANGUAGEPHRASE("tooltip_spawnbomber") );
                    break;
                                                                
                case WorldObject::TargetTypeLaunchNuke:         
                    img = g_resource->GetImage( "graphics/nuke.bmp" );          
                    tooltip->PutData( LANGUAGEPHRASE("tooltip_spawnnuke") );
                    break;
            }
            
            float lineX = ( actionCursorLongitude - predictedLongitude );
            float lineY = ( actionCursorLatitude - predictedLatitude );

            float angle = atan( -lineX / lineY );
            if( lineY < 0.0f ) angle += M_PI;

            g_renderer->SetBlendMode( Renderer::BlendModeAdditive );
            g_renderer2d->RotatingSprite( img, actionCursorLongitude, actionCursorLatitude, 
                                     spawnUnitSize/2.0f, spawnUnitSize/2.0f, 
                                     spawnUnitCol, angle );           
        }


        if( validCombatTarget == WorldObject::TargetTypeValid )
        {
            tooltip->PutData( LANGUAGEPHRASE("tooltip_attacK") );
        }

        if( selection->IsMovingObject() )
        {
            if( validMovementTarget == WorldObject::TargetTypeValid )
            {
                if( highlight && validCombatTarget == WorldObject::TargetTypeLand )
                {
                    tooltip->PutData( LANGUAGEPHRASE("tooltip_landobject") );
                }

                if( !fleet )
                {
                    tooltip->PutData( LANGUAGEPHRASE("tooltip_moveobject") );
                }
            }

            if( fleet && fleetValidMovement )
            {
                tooltip->PutData( LANGUAGEPHRASE("tooltip_movefleet") );                
            }

            if( fleet && !fleetValidMovement )
            {
                tooltip->PutData( LANGUAGEPHRASE("tooltip_cantmovehere") );
            }
        }


        if( selection )
        {
            tooltip->PutData( LANGUAGEPHRASE("tooltip_spacetodeselect") );
        }

        //
        // How many do we have left?

        if( spawnType > WorldObject::TargetTypeValid)
        {
            int numRemaining = -1;
            if( selection->m_states[selection->m_currentState]->m_numTimesPermitted > -1 )
            {
                numRemaining = selection->m_states[selection->m_currentState]->m_numTimesPermitted - selection->m_actionQueue.Size();
            }
            
            if( numRemaining >= 0 )
            {
                g_renderer->SetFont( "kremlin", true );
                g_renderer2d->Text( actionCursorLongitude-2, actionCursorLatitude, White, 2, "%d", numRemaining );
                g_renderer->SetFont();
            }
        }
        
        g_renderer->SetBlendMode( Renderer::BlendModeNormal );

        RenderWorldObjectTargets(selection);

        bool animateActionLine = !fleet || fleetValidMovement || validCombatTarget >= WorldObject::TargetTypeValid;

        RenderActionLine( predictedLongitude, predictedLatitude, 
                          actionCursorLongitude, actionCursorLatitude, 
                          actionCursorCol, 1.0f, animateActionLine );

        if( highlight && validCombatTarget )
        {
            RenderWorldObjectDetails( highlight );
        }

        if( selection->m_fleetId != -1 )
        {
            Fleet *fleet = team->GetFleet(selection->m_fleetId);
            for( int i = 0; i < fleet->m_fleetMembers.Size(); ++i )
            {
                WorldObject *obj = g_app->GetWorld()->GetWorldObject( fleet->m_fleetMembers[i] );
                if( obj &&
                    obj->m_objectId != selection->m_objectId )
                {
                    RenderWorldObjectTargets( obj, false );
                }
            }
        }


        //if( fleet )
        //{
        //    float sailDistance = g_app->GetWorld()->GetSailDistance( fleet->m_longitude, fleet->m_latitude, actionCursorLongitude, actionCursorLatitude );
        //    g_renderer->SetFont( NULL, true );
        //    g_renderer2d->Text( actionCursorLongitude, actionCursorLatitude, White, 3, "%2.2f", sailDistance );
        //    g_renderer->SetFont();
        //}
    }
    
    
    //
    // Render our current highlight (or state menu when open, even with selection)

    if( highlight && ( !selection || m_stateRenderTime > 0.0f ) )
    {
        float predictedLongitude = highlight->m_longitude.DoubleValue() + highlight->m_vel.x.DoubleValue() * g_predictionTime * timeScale;
        float predictedLatitude = highlight->m_latitude.DoubleValue() + highlight->m_vel.y.DoubleValue() * g_predictionTime  * timeScale;
        float size = 3.0f;
        if( GetZoomFactor() <= 0.25f )
        {
            size *= GetZoomFactor() * 4;
        }

        Team *team = g_app->GetWorld()->GetTeam(highlight->m_teamId);
        Colour col(128,128,128);
        if( team ) col = team->GetTeamColour();
        col.m_a = 150;

        g_renderer2d->Circle( predictedLongitude, predictedLatitude, size, 30, col, 1.5f );
        g_renderer2d->Circle( predictedLongitude, predictedLatitude, size, 30, Colour(255,255,255,100), 1.5f );

        RenderWorldObjectTargets(highlight);
        RenderWorldObjectDetails(highlight);


        if( highlight->m_teamId == g_app->GetWorld()->m_myTeamId &&
            !highlight->IsCityClass() &&
            !highlight->IsRadarClass() )
        {
            bool lmbDone = false;

            if( highlight->m_fleetId != -1 )
            {
                Fleet *fleet = g_app->GetWorld()->GetTeam(highlight->m_teamId)->GetFleet(highlight->m_fleetId);
                if( fleet && fleet->m_fleetMembers.Size() > 1 )
                {
                    tooltip->PutData( LANGUAGEPHRASE("tooltip_selectfleet") );
                    lmbDone = true;
                }
            }

            if( !lmbDone && !highlight->IsBallisticMissileClass() )
            {
                tooltip->PutData( LANGUAGEPHRASE("tooltip_selectobject") );
            }

            if( !highlight->IsBattleShipClass() &&
                !highlight->IsFighterClass() )
            {
                tooltip->PutData( LANGUAGEPHRASE("tooltip_setobjectstate") );
            }
        }
    }
#endif


    if( !highlight)
    {
        RenderEmptySpaceDetails(longitude, latitude);
    }


    //
    // Finish the tooltip swap

    if( m_tooltip )
    {
        delete m_tooltip;
        m_tooltip = NULL;
    }

    m_tooltip = tooltip;
}


void MapRenderer::RenderEmptySpaceDetails( float _mouseX, float _mouseY )
{
    int tooltipsEnabled = g_preferences->GetInt( PREFS_INTERFACE_TOOLTIPS );
    if( tooltipsEnabled && 
        m_tooltip &&
        m_tooltip->Size() )
    {
        float titleSize, textSize, gapSize;
        GetStatePositionSizes( &titleSize, &textSize, &gapSize );

        float boxH = (textSize+gapSize);
        float boxW = 18.0f * m_drawScale;
        float boxX = _mouseX - (boxW/3);
        float boxY = _mouseY - titleSize - titleSize;        
        float boxSep = gapSize;


        //
        // Fill box

        Colour windowColPrimary   = g_styleTable->GetPrimaryColour( STYLE_POPUP_BACKGROUND );
        Colour windowColSecondary = g_styleTable->GetSecondaryColour( STYLE_POPUP_BACKGROUND );
        Colour windowBorder       = g_styleTable->GetPrimaryColour( STYLE_POPUP_BORDER );    
        Colour fontCol            = g_styleTable->GetPrimaryColour( FONTSTYLE_POPUP_TITLE );
        bool windowOrientation    = g_styleTable->GetStyle( STYLE_POPUP_BACKGROUND )->m_horizontal;

        float alpha = (m_mouseIdleTime-1.0f);
        Clamp( alpha, 0.0f, 1.0f );

        windowColPrimary.m_a    *= alpha;
        windowColSecondary.m_a  *= alpha;
        windowBorder.m_a        *= alpha;
        fontCol.m_a             *= alpha;

        boxY += boxH;
        float totalBoxH = 0.0f;
        totalBoxH += m_tooltip->Size() * -boxH * 0.6f;


        g_renderer2d->RectFill( boxX, boxY, boxW, totalBoxH, windowColPrimary, windowColSecondary, windowOrientation );
        g_renderer2d->Rect( boxX, boxY, boxW, totalBoxH, windowBorder );


        //
        // Tooltip

        g_renderer->SetFont( "kremlin", true );

        float tooltipY = boxY - 0.5f * m_drawScale;
        RenderTooltip( &boxX, &tooltipY, &boxW, &boxH, &boxSep, alpha );

		g_renderer->SetFont();
    }
}


void MapRenderer::RenderTooltip( char *_tooltip )
{ 
    static char *s_previousTooltip = NULL;

    if( !s_previousTooltip ||
        strcmp(s_previousTooltip, _tooltip) != 0 )
    {
        // The tooltip has changed
        delete s_previousTooltip;
        s_previousTooltip = strdup(_tooltip);
        m_tooltipTimer = GetHighResTime();
    }

    

    int showTooltips = g_preferences->GetInt( PREFS_INTERFACE_TOOLTIPS );
    float timer = GetHighResTime() - m_tooltipTimer;

    if( showTooltips && 
        timer > 0.5f && 
        strlen(_tooltip) > 1 &&
        m_stateObjectId == -1 )
    {
        float alpha = (timer-0.5f);
        Clamp( alpha, 0.0f, 0.8f );

    
        //
        // Word wrap the tooltip

        float textH = 4 * m_zoomFactor;
        float maxW = 60 * m_zoomFactor;
		g_renderer->SetFont( "zerothre" );
        MultiLineText wrapped( _tooltip, maxW, textH, false );
    
        float widestLine = 0.0f;
        int numLines = wrapped.Size();
        for( int i = 0; i < numLines; ++i )
        {
            float thisWidth = g_renderer2d->TextWidth( wrapped[i], textH );
            widestLine = max( widestLine, thisWidth );
        }


        //
        // Render the box

        float longitude;
        float latitude;    
        ConvertPixelsToAngle( g_inputManager->m_mouseX, g_inputManager->m_mouseY, &longitude, &latitude );

        float boxX = longitude + 5 * m_zoomFactor;
        float boxY = latitude - 2 * m_zoomFactor;
        float boxH = textH * numLines;
        float boxW = widestLine + 4 * m_zoomFactor;

        //g_renderer2d->RectFill( boxX, boxY, boxW, boxH, Colour(130,130,50,alpha*255) );

        Colour windowColP  = g_styleTable->GetPrimaryColour( STYLE_TOOLTIP_BACKGROUND );
        Colour windowColS  = g_styleTable->GetSecondaryColour( STYLE_TOOLTIP_BACKGROUND );
        Colour borderCol   = g_styleTable->GetPrimaryColour( STYLE_TOOLTIP_BORDER );
    
        bool alignment = g_styleTable->GetStyle(STYLE_TOOLTIP_BACKGROUND)->m_horizontal;

        windowColP.m_a *= alpha;
        windowColS.m_a *= alpha;
        borderCol.m_a *= alpha;

        g_renderer2d->RectFill ( boxX, boxY, boxW, boxH, windowColS, windowColP, alignment );
        g_renderer2d->Rect     ( boxX, boxY, boxW, boxH, borderCol);

    
        //
        // Render the text
    
        g_renderer->SetFont( NULL, true );

        float textX = boxX + 2 * m_zoomFactor;
        float y = boxY - textH;
        for( int i = numLines-1; i >= 0; --i )
        {
            g_renderer2d->Text( textX, y+=textH, Colour(255,255,255,alpha*255), textH, wrapped[i] );
        }

        g_renderer->SetFont();
    }
}


void MapRenderer::RenderActionLine( float fromLong, float fromLat, float toLong, float toLat, Colour col, float width, bool animate )
{
#ifndef NON_PLAYABLE
    float distance      = g_app->GetWorld()->GetDistance( Fixed::FromDouble(fromLong), Fixed::FromDouble(fromLat),
														  Fixed::FromDouble(toLong), Fixed::FromDouble(toLat), true ).DoubleValue();
    float distanceLeft  = g_app->GetWorld()->GetDistance( Fixed::FromDouble(fromLong), Fixed::FromDouble(fromLat),
														  Fixed::FromDouble(toLong - 360), Fixed::FromDouble(toLat), true ).DoubleValue();
    float distanceRight = g_app->GetWorld()->GetDistance( Fixed::FromDouble(fromLong), Fixed::FromDouble(fromLat),
														  Fixed::FromDouble(toLong + 360), Fixed::FromDouble(toLat), true ).DoubleValue();
    if( distanceLeft < distance )
    {
        if( fromLong < 0.0f && toLong > 0.0f )
        {
            toLong -= 360.0f;
        }
    }
    else if( distanceRight < distance )
    {
        if( fromLong > 0.0f && toLong < 0.0f )
        {
            toLong += 360.0f;
        }
    }

    bool isFirstIteration = (m_seamIteration == 0);

    g_renderer2d->Line( fromLong, fromLat, toLong, toLat, col, width );
    
    if( isFirstIteration )
    {
        g_renderer2d->Line( fromLong+GetLongitudeMod(), fromLat, toLong+GetLongitudeMod(), toLat, col, width );
    }

    if( animate )
    {
        float timeFrac = fmodf( GetHighResTime(), 1.0f );
        float factor1 = timeFrac;
        float factor2 = timeFrac + 0.2f;
        
        Vector3<float> lineVector( toLong - fromLong, toLat - fromLat, 0 );
        
        if( factor2 <= 1.0f )
        {
            //
            // Normal case: draw single segment

            Vector3<float> fromVector = Vector3<float>(fromLong,fromLat,0) + lineVector * factor1;
            Vector3<float> toVector = Vector3<float>(fromLong,fromLat,0) + lineVector * factor2;
            g_renderer2d->Line( fromVector.x, fromVector.y, toVector.x, toVector.y, col, width );
        }
        else
        {
            //
            // Wrap around case: draw two segments
            // First segment: from factor1 to end of line 

            Vector3<float> fromVector1 = Vector3<float>(fromLong,fromLat,0) + lineVector * factor1;
            Vector3<float> toVector1 = Vector3<float>(fromLong,fromLat,0) + lineVector * 1.0f;
            g_renderer2d->Line( fromVector1.x, fromVector1.y, toVector1.x, toVector1.y, col, width );
            
            //
            // Second segment: from start of line to wrapped amount

            Vector3<float> fromVector2 = Vector3<float>(fromLong,fromLat,0) + lineVector * 0.0f;
            Vector3<float> toVector2 = Vector3<float>(fromLong,fromLat,0) + lineVector * (factor2 - 1.0f);
            g_renderer2d->Line( fromVector2.x, fromVector2.y, toVector2.x, toVector2.y, col, width );
        }
    }

#endif
}


void MapRenderer::RenderWorldObjectTargets( WorldObject *wobj, bool maxRanges )
{
#ifndef NON_PLAYABLE
    bool isFirstSeamIteration = ( m_seamIteration == 0 );

    if( wobj->m_teamId == g_app->GetWorld()->m_myTeamId ||
        g_app->GetWorld()->m_myTeamId == -1 ||
        wobj->IsBallisticMissileClass() )
    {
        float predictedLongitude = wobj->m_longitude.DoubleValue() +
								   wobj->m_vel.x.DoubleValue() * g_predictionTime * g_app->GetWorld()->GetTimeScaleFactor().DoubleValue();
        float predictedLatitude = wobj->m_latitude.DoubleValue() +
								  wobj->m_vel.y.DoubleValue() * g_predictionTime * g_app->GetWorld()->GetTimeScaleFactor().DoubleValue();

        if( maxRanges )
        {
            int renderTooltip = g_preferences->GetInt( PREFS_INTERFACE_TOOLTIPS );

            //
            // Render red circle for our action range

            float range = wobj->GetActionRange().DoubleValue();
            if( m_currentStateId != -1 )
            {
                WorldObject *obj = g_app->GetWorld()->GetWorldObject( g_app->GetWorldRenderer()->GetCurrentHighlightId() );
                if( obj )
                {
                    if( obj->m_states.ValidIndex( m_currentStateId ) )
                    {
                        range = obj->m_states[m_currentStateId]->m_actionRange.DoubleValue();
                    }
                }

            }
            if( range <= 180.0f && range > 0.0f )
            {
                Colour col(255,0,0,255);
                g_renderer2d->Circle( predictedLongitude, predictedLatitude, range, 40, col, 1.0f );
                
                if( renderTooltip )
                {
                    g_renderer->SetFont( "kremlin", true );
                                            g_renderer2d->TextCentreSimple( predictedLongitude, predictedLatitude+range, col, 1, LANGUAGEPHRASE("dialog_mapr_combat_range") );
                    g_renderer->SetFont();
                }
            }


            //
            // Render blue circle for our movement range

            if( wobj->IsMovingObject() )
            {
                MovingObject *mobj = (MovingObject *) wobj;
                if( mobj->m_range <= 180 && mobj->m_range > 0 )
                {                    
                    float predictedRange = mobj->m_range.DoubleValue();

                    Colour col(0,0,255,255);
                    g_renderer2d->Circle( predictedLongitude, predictedLatitude, predictedRange, 50, col, 1.5f );

                    if( renderTooltip )
                    {
                        g_renderer->SetFont( "kremlin", true );
                        g_renderer2d->TextCentreSimple( predictedLongitude, predictedLatitude+predictedRange, col, 1, LANGUAGEPHRASE("dialog_mapr_fuel_range") );
                        g_renderer->SetFont();
                    }
                }
            }   
        }
        
        
        //
        // Render action line to our combat target (red = nuke, orange = LACM/conventional)
        // Unit/cursor icons use additive blend so only the white parts show (no black background)
        g_renderer->SetBlendMode( Renderer::BlendModeAdditive );

        int targetObjectId = wobj->GetTargetObjectId();
        WorldObject *targetObject = g_app->GetWorld()->GetWorldObject( targetObjectId );
        if( targetObject )
        {
            float TpredictedLongitude = targetObject->m_longitude.DoubleValue() + 
										targetObject->m_vel.x.DoubleValue() * g_predictionTime *
										g_app->GetWorld()->GetTimeScaleFactor().DoubleValue();
            float TpredictedLatitude = targetObject->m_latitude.DoubleValue() + 
									   targetObject->m_vel.y.DoubleValue() * g_predictionTime * 
									   g_app->GetWorld()->GetTimeScaleFactor().DoubleValue();

            Colour actionCursorCol = wobj->UsingNukes() ? Colour( 255, 0, 0, 150 ) : Colour( 255, 165, 0, 150 );  // Red: nuke / Orange: LACM, conventional
            float actionCursorSize = 2.0f;
            float actionCursorAngle = g_gameTime * -1.0f;

            Image *img = g_resource->GetImage( "graphics/cursor_target.bmp" );
            g_renderer2d->RotatingSprite( img, TpredictedLongitude, TpredictedLatitude, 
                                actionCursorSize, actionCursorSize, 
                                actionCursorCol, actionCursorAngle );
            if( isFirstSeamIteration )
            {
                g_renderer2d->RotatingSprite( img, TpredictedLongitude + GetLongitudeMod(), TpredictedLatitude, 
                                    actionCursorSize, actionCursorSize, 
                                    actionCursorCol, actionCursorAngle );
            }

            RenderActionLine( predictedLongitude, predictedLatitude, 
                              TpredictedLongitude, TpredictedLatitude, 
                              actionCursorCol, 1.0f );
        }
        

        //
        // Render action line to our movement target (red = nuke in flight, blue = move, green = landing, lavender = bomber nuke mode waypoint only)

        if( !targetObject && wobj->IsMovingObject() )
        {
            MovingObject *mobj = (MovingObject *) wobj;
            float actionCursorLongitude = mobj->m_targetLongitude.DoubleValue();
            float actionCursorLatitude = mobj->m_targetLatitude.DoubleValue();

            if( mobj->m_fleetId != -1 )
            {
                Fleet *fleet = g_app->GetWorld()->GetTeam(mobj->m_teamId)->GetFleet(mobj->m_fleetId);
                if( fleet && 
                    fleet->m_targetLongitude != 0 && 
                    fleet->m_targetLatitude != 0 )
                {
                    //predictedLongitude = fleet->m_longitude;
                    //predictedLatitude = fleet->m_latitude;
                    actionCursorLongitude = fleet->m_targetLongitude.DoubleValue();
                    actionCursorLatitude = fleet->m_targetLatitude.DoubleValue();                    
                }
            }
            if( actionCursorLongitude != 0.0f || actionCursorLatitude != 0.0f )
            {
                if( mobj->IsBallisticMissileClass() &&
                    wobj->m_teamId != g_app->GetWorld()->m_myTeamId &&
                    g_app->GetWorld()->m_myTeamId != -1 )
                {
                    Nuke *nuke = (Nuke *)mobj;
                    RenderNukeImpactZone( nuke, actionCursorLongitude, actionCursorLatitude,
                                         predictedLongitude, predictedLatitude );
                }
                else
                {
                Colour actionCursorCol( 0, 0, 255, 150 );
                float actionCursorSize = 2.0f;
                float actionCursorAngle = 0;

                if( mobj->IsBallisticMissileClass() )
                {
                    actionCursorCol.Set( 255, mobj->IsNuke() ? 0 : 165, 0, 150 );   // Red: nuke ballistic / Orange: non-nuke ballistic (future)
                }
                else if( mobj->IsAircraft() && mobj->m_isLanding != -1 )
                {
                    actionCursorCol.Set( 0, 255, 0, 150 );   // Green: landing
                }
                else if( mobj->IsBomberClass() && mobj->m_currentState == 1 )
                {
                    // Bomber in nuke mode but no nuke target set: waypoint only (Lavender)
                    actionCursorCol.Set( 187, 127, 255, 150 );
                }

                Image *img = g_resource->GetImage( "graphics/cursor_target.bmp" );
                g_renderer2d->RotatingSprite( img, actionCursorLongitude, actionCursorLatitude, 
                                    actionCursorSize, actionCursorSize, 
                                    actionCursorCol, actionCursorAngle );
                if( isFirstSeamIteration )
                {
                    g_renderer2d->RotatingSprite( img, actionCursorLongitude + GetLongitudeMod(), actionCursorLatitude, 
                                        actionCursorSize, actionCursorSize, 
                                        actionCursorCol, actionCursorAngle );
                }

                RenderActionLine( predictedLongitude, predictedLatitude, 
                                  actionCursorLongitude, actionCursorLatitude, 
                                  actionCursorCol, 1.0f );
                }
            }
        }


        //
        // Render our action queue (red = nuke ballistic, orange = LACM/non-nuke ballistic, gray = standby, blue = waypoint)

        if( wobj->m_actionQueue.Size() )
        {
            Image *img = NULL;
            float size = 1.0f;

            switch( wobj->m_classType )
            {
                case WorldObject::ClassTypeAirbase:
                case WorldObject::ClassTypeCarrier:
                    if( wobj->m_currentState == 0 ) img = g_resource->GetImage( "graphics/fighter.bmp" );
                    if( wobj->m_currentState == 1 ) img = g_resource->GetImage( "graphics/bomber.bmp" );
                    break;

                case WorldObject::ClassTypeSilo:
                    if( wobj->m_currentState == 0 || wobj->m_currentState == 1 ) img = g_resource->GetImage( "graphics/nuke.bmp" );
                    break;

                case WorldObject::ClassTypeSub:
                    if( wobj->m_currentState == 2 || wobj->m_currentState == 3 ) img = g_resource->GetImage( "graphics/nuke.bmp" );
                    break;

                case WorldObject::ClassTypeBattleShip:
                    if( wobj->m_currentState == 1 ) img = g_resource->GetImage( "graphics/lacm.bmp" );
                    break;

                case WorldObject::ClassTypeBomber:
                    if( wobj->m_currentState == 0 || wobj->m_currentState == 1 ) img = g_resource->GetImage( "graphics/nuke.bmp" );
                    break;
            }
            
            if( img )
            {
                for( int i = 0; i < wobj->m_actionQueue.Size(); ++i )
                {
                    ActionOrder *order = wobj->m_actionQueue[i];
                    float targetLongitude = order->m_longitude.DoubleValue();
                    float targetLatitude = order->m_latitude.DoubleValue();

                    WorldObject *targetObject = g_app->GetWorld()->GetWorldObject( order->m_targetObjectId );
                    if( targetObject )
                    {
                        targetLongitude = targetObject->m_longitude.DoubleValue() +
										   targetObject->m_vel.x.DoubleValue() * g_predictionTime *
										   g_app->GetWorld()->GetTimeScaleFactor().DoubleValue();
                        targetLatitude = targetObject->m_latitude.DoubleValue() +
										 targetObject->m_vel.y.DoubleValue() * g_predictionTime *
										 g_app->GetWorld()->GetTimeScaleFactor().DoubleValue();
                    }

                    float lineX = ( targetLongitude - predictedLongitude );
                    float lineY = ( targetLatitude - predictedLatitude );

                    float angle = atan( -lineX / lineY );
                    if( lineY < 0.0f ) angle += M_PI;
                
                    bool isActiveTarget = ( i == 0 );
                    bool isStandbyQueue = ( wobj->IsActionQueueable() && !wobj->UsingNukes() );
                    Colour iconCol = isActiveTarget ? Colour( 255, 255, 255, 180 ) : Colour( 180, 180, 180, 100 );
                    if( isStandbyQueue )
                        iconCol = isActiveTarget ? Colour( 140, 140, 140, 150 ) : Colour( 100, 100, 100, 100 );
                    float lineWidth = isActiveTarget ? 1.0f : 0.5f;
                    Colour lineCol;
                    if( isStandbyQueue )
                        lineCol = isActiveTarget ? Colour( 128, 128, 128, 150 ) : Colour( 100, 100, 100, 100 );   // Gray: standby queue
                    else if( wobj->UsingNukes() )
                        lineCol = isActiveTarget ? Colour( 255, 0, 0, 180 ) : Colour( 200, 50, 50, 180 );        // Red: nuke ballistic (and future nuclear cruise)
                    else if( wobj->IsAircraftLauncher() && order->m_targetObjectId == -1 )
                        lineCol = isActiveTarget ? Colour( 0, 0, 255, 180 ) : Colour( 100, 100, 255, 150 );     // Blue (launch to location)
                    else
                        lineCol = isActiveTarget ? Colour( 255, 165, 0, 180 ) : Colour( 255, 200, 80, 180 );    // Orange/yellow: LACM, non-nuke ballistic

                    g_renderer2d->RotatingSprite( img, targetLongitude, targetLatitude,
                                                  size, size, iconCol, angle );
                    if( isFirstSeamIteration )
                    {
                        g_renderer2d->RotatingSprite( img, targetLongitude + GetLongitudeMod(), targetLatitude,
                                                      size, size, iconCol, angle );
                    }
                                                  
                    RenderActionLine( predictedLongitude, predictedLatitude,
                                      targetLongitude, targetLatitude,
                                      lineCol, lineWidth );
                }
            }
        }

        g_renderer->SetBlendMode( Renderer::BlendModeNormal );
    }
#endif
}

void MapRenderer::RenderDragSelectionMarquee()
{
    if( m_isDraggingSelect )
    {
        Colour boxCol = Colour( 100, 200, 255, 180 );
        Colour fillCol = Colour( 80, 160, 220, 40 );

        float x1 = m_dragStartLong.DoubleValue();
        float y1 = m_dragStartLat.DoubleValue();
        float x2 = m_dragEndLong.DoubleValue();
        float y2 = m_dragEndLat.DoubleValue();

        if( x2 < x1 ) { float t=x1; x1=x2; x2=t; }
        if( y2 < y1 ) { float t=y1; y1=y2; y2=t; }

        g_renderer2d->RectFill( x1, y1, (x2-x1), (y2-y1), fillCol );
        g_renderer2d->Rect( x1, y1, (x2-x1), (y2-y1), boxCol );
    }
}

void MapRenderer::RenderUnitHighlight( int _objectId )
{
    WorldObject *obj = g_app->GetWorld()->GetWorldObject( _objectId );
    if( obj )
    {
        float predictedLongitude = obj->m_longitude.DoubleValue() + 
								   obj->m_vel.x.DoubleValue() * g_predictionTime * g_app->GetWorld()->GetTimeScaleFactor().DoubleValue();
        float predictedLatitude = obj->m_latitude.DoubleValue() + 
							      obj->m_vel.y.DoubleValue() * g_predictionTime  * g_app->GetWorld()->GetTimeScaleFactor().DoubleValue();
        
        float size = 5.0f;
        if( GetZoomFactor() <=0.25f )
        {
            size *= GetZoomFactor() * 4;
        }

        Colour col = g_app->GetWorld()->GetTeam( obj->m_teamId )->GetTeamColour();
        col.m_a = 255;
        float rectWidth = 2.0f;
        float lineWidth = 1.0f;

        if( fmod((float)g_gameTime*2, 2.0f) < 1.0f ) col.m_a = 100;

        g_renderer2d->Line(predictedLongitude - size/2 - 5.0f, predictedLatitude, 
                         predictedLongitude - size/2, predictedLatitude, col, lineWidth );
        g_renderer2d->Line(predictedLongitude + size/2 + 5.0f, predictedLatitude, 
                         predictedLongitude + size/2, predictedLatitude, col, lineWidth );
        g_renderer2d->Line(predictedLongitude, predictedLatitude + size/2 + 5.0f, 
                         predictedLongitude, predictedLatitude + size/2, col, lineWidth );
        g_renderer2d->Line(predictedLongitude, predictedLatitude - size/2 - 5.0f, 
                         predictedLongitude, predictedLatitude - size/2, col, lineWidth );
        g_renderer2d->Rect( predictedLongitude - size/2, 
                          predictedLatitude - size/2, size, size, col, rectWidth );
    }
}

void MapRenderer::RenderNukeUnits()
{
    Team *team = g_app->GetWorld()->GetMyTeam();
    if( team )
    {
        for( int i = 0; i < g_app->GetWorld()->m_objects.Size(); ++i )
        {
            if( g_app->GetWorld()->m_objects.ValidIndex(i) )
            {
                WorldObject *obj = g_app->GetWorld()->m_objects[i];
                if( obj->m_teamId == team->m_teamId )
                {
                    int nukeCount = 0;

                    switch( obj->m_classType )
                    {
                        case WorldObject::ClassTypeSilo:
                            nukeCount = obj->m_states[1]->m_numTimesPermitted;
                            break;

                        case WorldObject::ClassTypeSub:
                            nukeCount = obj->m_states[3]->m_numTimesPermitted;
                            break;

                        case WorldObject::ClassTypeBomber:
                            nukeCount = obj->m_states[1]->m_numTimesPermitted;
                            break;

                        case WorldObject::ClassTypeAirbase:
                        case WorldObject::ClassTypeCarrier:
                            nukeCount = ( obj->m_states[1]->m_numTimesPermitted - ( obj->UsingNukes() ? obj->m_actionQueue.Size() : 0 ) ) * 2;
                            break;
                    }

                    if( obj->UsingNukes() && !obj->IsAircraftLauncher() )
                    {
                        nukeCount -= obj->m_actionQueue.Size();
                    }

                    if( nukeCount > 0 )
                    {
                        RenderUnitHighlight( obj->m_objectId );
                    }
                }
            }
        }
    }
}


void MapRenderer::RenderLowDetailCoastlines()
{
    START_PROFILE( "Coastlines" );

    Image *bmpWorld = g_resource->GetImage( "graphics/map.bmp" );
    Colour col = White;

    g_renderer2d->StaticSprite( bmpWorld, -180, 100, 360, -200, col );

    END_PROFILE( "Coastlines" );
}


void MapRenderer::RenderCoastlines()
{
    START_PROFILE( "Coastlines" );

    float mapColourFader = 1.0f;
    Team *myTeam = g_app->GetWorld()->GetMyTeam();
    if( myTeam ) mapColourFader = myTeam->m_teamColourFader;

    Colour lineColour           = g_styleTable->GetPrimaryColour( STYLE_WORLD_COASTLINES );
    Colour desaturatedColour    = g_styleTable->GetSecondaryColour( STYLE_WORLD_COASTLINES );
    lineColour = lineColour * mapColourFader + desaturatedColour * (1-mapColourFader);

#ifndef TARGET_EMSCRIPTEN
    g_renderer->SetLineWidth(g_preferences->GetFloat(PREFS_GRAPHICS_COASTLINE_THICKNESS));
#endif

    //
    // Check if VBO exists and is valid, if not build it

    if (!g_megavbo2d->IsMegaVBOValid("MapCoastlines")) {
        LList<Island *> *list = &g_app->GetEarthData()->m_islands;

        Colour coastlineColor = g_styleTable->GetPrimaryColour( STYLE_WORLD_COASTLINES );
        
        g_megavbo2d->BeginMegaVBO( "MapCoastlines", coastlineColor );
        
        for( int i = 0; i < list->Size(); ++i )
        {
            Island *island = list->GetData(i);
            AppDebugAssert( island );

            //
            // Convert island points to vertex array

            int pointCount = island->m_points.Size();
            if (pointCount >= 2) {
                float* vertices = new float[pointCount * 2];
                for( int j = 0; j < pointCount; j++ )
                {
                    Vector3<float> thePoint = *island->m_points[j];
                    vertices[j * 2] = thePoint.x;
                    vertices[j * 2 + 1] = thePoint.y;
                }
                
                //
                // Add this island's line strip to the mega-VBO

                g_megavbo2d->AddLineStripToMegaVBO( vertices, pointCount );
                delete[] vertices;
            }
        }
        
        g_megavbo2d->EndMegaVBO();
    }
    
    g_megavbo2d->RenderMegaVBO( "MapCoastlines" );

    END_PROFILE( "Coastlines" );
}

void MapRenderer::RenderBorders()
{
    START_PROFILE( "Borders" );

    float mapColourFader = 1.0f;
    Team *myTeam = g_app->GetWorld()->GetMyTeam();
    if( myTeam ) mapColourFader = myTeam->m_teamColourFader;
    
    Colour lineColour = g_styleTable->GetPrimaryColour( STYLE_WORLD_BORDERS );
    Colour desaturatedColour = g_styleTable->GetSecondaryColour( STYLE_WORLD_BORDERS );
    lineColour = lineColour * mapColourFader + desaturatedColour * (1-mapColourFader);

#ifndef TARGET_EMSCRIPTEN
    g_renderer->SetLineWidth(g_preferences->GetFloat(PREFS_GRAPHICS_BORDER_THICKNESS));
#endif
    
    g_renderer->SetBlendMode( Renderer::BlendModeNormal );
    
    //
    // Check if VBO exists and is valid, if not build it

    if (!g_megavbo2d->IsMegaVBOValid("MapBorders")) {
        
        g_megavbo2d->BeginMegaVBO( "MapBorders", lineColour );
        
        for( int i = 0; i < g_app->GetEarthData()->m_borders.Size(); ++i )
        {
            Island *island = g_app->GetEarthData()->m_borders[i];
            AppDebugAssert( island );

            //
            // Convert border points to vertex array

            int pointCount = island->m_points.Size();
            if (pointCount >= 2) {
                float* vertices = new float[pointCount * 2];
                for( int j = 0; j < pointCount; j++ )
                {
                    Vector3<float> thePoint = *island->m_points[j];
                    vertices[j * 2] = thePoint.x;
                    vertices[j * 2 + 1] = thePoint.y;
                }
                
                g_megavbo2d->AddLineStripToMegaVBO( vertices, pointCount );
                delete[] vertices;
            }
        }
        
        g_megavbo2d->EndMegaVBO();
    }

    g_megavbo2d->RenderMegaVBO( "MapBorders" );

    END_PROFILE( "Borders" );
}

void MapRenderer::RenderObjects()
{
    START_PROFILE( "Objects" );

    int myTeamId = g_app->GetWorld()->m_myTeamId;
    
    //
    // set font once for all status text

    g_renderer->SetFont( "kremlin", true );

    //
    // set blend mode once for all objects

    g_renderer->SetBlendMode( Renderer::BlendModeAdditive );
     
    for( int i = 0; i < g_app->GetWorld()->m_objects.Size(); ++i )
    {
        if( g_app->GetWorld()->m_objects.ValidIndex(i) )
        {            
            WorldObject *wobj = g_app->GetWorld()->m_objects[i];
            START_PROFILE( WorldObject::GetName(wobj->m_type) );

            bool onScreen = IsOnScreen( wobj->m_longitude.DoubleValue(), wobj->m_latitude.DoubleValue() );
            if( onScreen || wobj->IsBallisticMissileClass() )
            {
                if( myTeamId == -1 ||
                    wobj->m_teamId == myTeamId ||
                    wobj->m_visible[myTeamId] ||
                    g_app->GetWorldRenderer()->CanRenderEverything() )
                {
                    wobj->Render2D();
                }
                else
                {
                    wobj->RenderGhost2D(myTeamId);
                }
            }

#ifndef NON_PLAYABLE
            if( g_app->GetWorldRenderer()->m_showOrders )
            {
                if(myTeamId == -1 || wobj->m_teamId == myTeamId )                
                {
                    RenderWorldObjectTargets(wobj, false);                // This shows ALL object orders on screen at once
                }
            }


            //
            // Render num nukes / LACM on the way

            if( onScreen )
            {
                float yPos = wobj->m_latitude.DoubleValue() + 1.6f;
                float textSize = 0.75f;

                if( wobj->m_numNukesInFlight || wobj->m_numNukesInQueue )
                {
                    Colour col(255,0,0,255);
                    if( !wobj->m_numNukesInFlight ) col.m_a = 100;
                    float iconSize = 2.0f;
                    if( wobj->m_numNukesInFlight ) iconSize += sinf(g_gameTime*10) * 0.2f;

                    Image *img = g_resource->GetImage( "graphics/nukesymbol.bmp" );
                    g_renderer2d->RotatingSprite( img, wobj->m_longitude.DoubleValue(), wobj->m_latitude.DoubleValue(), iconSize, iconSize, col, 0 );

                    if( wobj->m_numNukesInQueue )
                    {
                        col.m_a = 100;
                        char caption[128];
                        strcpy( caption, LANGUAGEPHRASE("dialog_mapr_nukes_in_queue") );
                        LPREPLACEINTEGERFLAG( 'N', wobj->m_numNukesInQueue, caption );
                        g_renderer2d->TextCentreSimple( wobj->m_longitude.DoubleValue(), yPos, col, textSize, caption );
                        yPos += 0.5f;
                    }
                    if( wobj->m_numNukesInFlight )
                    {
                        col.m_a = 255;
                        char caption[128];
                        strcpy( caption, LANGUAGEPHRASE("dialog_mapr_nukes_in_flight") );
                        LPREPLACEINTEGERFLAG( 'N', wobj->m_numNukesInFlight, caption );
                        g_renderer2d->TextCentreSimple( wobj->m_longitude.DoubleValue(), yPos, col, textSize, caption );
                        yPos += 0.5f;
                    }
                }

                if( wobj->m_numLACMInFlight || wobj->m_numLACMInQueue )
                {
                    Colour col(255,165,0,255);
                    if( !wobj->m_numLACMInFlight ) col.m_a = 100;
                    float iconSize = 2.0f;
                    if( wobj->m_numLACMInFlight ) iconSize += sinf(g_gameTime*10) * 0.2f;

                    Image *img = g_resource->GetImage( "graphics/lacmsymbol.bmp" );
                    g_renderer2d->RotatingSprite( img, wobj->m_longitude.DoubleValue(), wobj->m_latitude.DoubleValue(), iconSize, iconSize, col, 0 );

                    if( wobj->m_numLACMInQueue )
                    {
                        col.m_a = 100;
                        char caption[128];
                        strcpy( caption, LANGUAGEPHRASE("dialog_mapr_lacm_in_queue") );
                        LPREPLACEINTEGERFLAG( 'N', wobj->m_numLACMInQueue, caption );
                        g_renderer2d->TextCentreSimple( wobj->m_longitude.DoubleValue(), yPos, col, textSize, caption );
                        yPos += 0.5f;
                    }
                    if( wobj->m_numLACMInFlight )
                    {
                        col.m_a = 255;
                        char caption[128];
                        strcpy( caption, LANGUAGEPHRASE("dialog_mapr_lacm_in_flight") );
                        LPREPLACEINTEGERFLAG( 'N', wobj->m_numLACMInFlight, caption );
                        g_renderer2d->TextCentreSimple( wobj->m_longitude.DoubleValue(), yPos, col, textSize, caption );
                    }
                }
            }
#endif
            END_PROFILE( WorldObject::GetName(wobj->m_type) );
        }
    }
    
    // Reset font once after all objects instead of per-object
    g_renderer->SetFont();

#ifndef NON_PLAYABLE
    WorldObject *selection = g_app->GetWorld()->GetWorldObject(g_app->GetWorldRenderer()->GetCurrentSelectionId());

    if( selection  )
    {
        if( selection->m_teamId == myTeamId )
        {
            if( selection->IsMovingObject() )
            {
                float longitude, latitude;
                ConvertPixelsToAngle( g_inputManager->m_mouseX, g_inputManager->m_mouseY, &longitude, &latitude );
                MovingObject *mobj = (MovingObject *)selection;
                if( mobj->m_movementType == MovingObject::MovementTypeSea &&
                    mobj->IsValidPosition( Fixed::FromDouble(longitude), Fixed::FromDouble(latitude) ) &&
                    g_app->GetWorldRenderer()->GetCurrentSelectionId() != g_app->GetWorldRenderer()->GetCurrentHighlightId())
                {
                    Fleet *fleet = g_app->GetWorld()->GetTeam( mobj->m_teamId )->GetFleet( mobj->m_fleetId );
                    if( fleet )
                    {
                        fleet->CreateBlip();
                    }
                }
            }
        }
    }

    WorldObject *highlight = g_app->GetWorld()->GetWorldObject( g_app->GetWorldRenderer()->GetCurrentHighlightId() );
 
    if( highlight  )
    {
        if(g_app->GetWorldRenderer()->GetCurrentHighlightId() != g_app->GetWorldRenderer()->GetCurrentSelectionId() )
        {
            if( highlight->m_teamId == myTeamId )
            {
                if( highlight->IsMovingObject() )
                {
                    MovingObject *mobj = (MovingObject *)highlight;
                    if( mobj->m_movementType == MovingObject::MovementTypeSea )
                    {
                        Fleet *fleet = g_app->GetWorld()->GetTeam( mobj->m_teamId )->GetFleet( mobj->m_fleetId );
                        if( fleet )
                        {
                            fleet->CreateBlip();
                        }
                    }
                }
            }
        }
    }
#endif

    END_PROFILE( "Objects" );
}

void MapRenderer::RenderBlips()
{
    g_renderer->SetBlendMode( Renderer::BlendModeAdditive );
    
    int teamId = g_app->GetWorld()->m_myTeamId;
    Team *team = g_app->GetWorld()->GetTeam( teamId );
    if( team )
    {
        for( int i = 0; i < team->m_fleets.Size(); ++i )
        {
            Fleet *fleet = team->GetFleet(i);
            if( fleet )
            {
                for( int j = 0; j < fleet->m_movementBlips.Size(); ++j )
                {
                    Blip *blip = fleet->m_movementBlips[j];
                    if( blip->Update() )
                    {
                        fleet->m_movementBlips.RemoveData(j);
                        j--;
                        delete blip;
                    }
                }
            }
        }
    }
}

void MapRenderer::RenderCities()
{
    START_PROFILE( "Cities" );

    g_renderer->SetFont( "kremlin", true );

    g_renderer->SetBlendMode( Renderer::BlendModeAdditive );
    Image *cityImg = g_resource->GetImage( "graphics/city.bmp" );


    //
    // City icons

    for( int i = 0; i < g_app->GetWorld()->m_cities.Size(); ++i )
    {
        City *city = g_app->GetWorld()->m_cities.GetData(i);

        if( city->m_capital || city->m_teamId != -1 )
        {            
            if( IsOnScreen( city->m_longitude.DoubleValue(), city->m_latitude.DoubleValue() ) )
            {
                float size = sqrtf( sqrtf((float) city->m_population) ) / 25.0f;
                float textSize = size;
                if( m_zoomFactor <= 0.2f )
                {
                    size *= m_zoomFactor * 5;
                }

                Colour col(100,100,100,100);
                if( city->m_teamId > -1 ) 
                {
                    col = g_app->GetWorld()->GetTeam(city->m_teamId)->GetTeamColour();
                }                            
                col.m_a = 200.0f * ( 1.0f - min(0.8f, m_zoomFactor) );
                            
                g_renderer2d->StaticSprite( cityImg,
                                           city->m_longitude.DoubleValue()-size/2, 
                                           city->m_latitude.DoubleValue()-size/2,
                                           size, size, col );

            
                //
                // Nuke / LACM inbound icons

                float yPos = city->m_latitude.DoubleValue() + 1.6f;
                float inboundTextSize = 0.75f;

                if( city->m_numNukesInFlight || city->m_numNukesInQueue )
                {
                    Colour col(255,0,0,255);
                    if( !city->m_numNukesInFlight ) col.m_a = 100;
                    float iconSize = 2.0f;
                    if( city->m_numNukesInFlight ) iconSize += sinf(g_gameTime*10) * 0.2f;

                    Image *img = g_resource->GetImage( "graphics/nukesymbol.bmp" );
                    g_renderer2d->StaticSprite( img, city->m_longitude.DoubleValue() - iconSize/2, city->m_latitude.DoubleValue() - iconSize/2, iconSize, iconSize, col );

                    if( city->m_numNukesInQueue )
                    {
                        col.m_a = 100;
                        char caption[128];
                        strcpy( caption, LANGUAGEPHRASE("dialog_mapr_nukes_in_queue") );
                        LPREPLACEINTEGERFLAG( 'N', city->m_numNukesInQueue, caption );
                        g_renderer2d->TextCentreSimple( city->m_longitude.DoubleValue(), yPos, col, inboundTextSize, caption );
                        yPos += 0.5f;
                    }
                    if( city->m_numNukesInFlight )
                    {
                        col.m_a = 255;
                        char caption[128];
                        strcpy( caption, LANGUAGEPHRASE("dialog_mapr_nukes_in_flight") );
                        LPREPLACEINTEGERFLAG( 'N', city->m_numNukesInFlight, caption );
                        g_renderer2d->TextCentreSimple( city->m_longitude.DoubleValue(), yPos, col, inboundTextSize, caption );
                        yPos += 0.5f;
                    }
                }

                if( city->m_numLACMInFlight || city->m_numLACMInQueue )
                {
                    Colour col(255,165,0,255);
                    if( !city->m_numLACMInFlight ) col.m_a = 100;
                    float iconSize = 2.0f;
                    if( city->m_numLACMInFlight ) iconSize += sinf(g_gameTime*10) * 0.2f;

                    Image *img = g_resource->GetImage( "graphics/lacmsymbol.bmp" );
                    g_renderer2d->StaticSprite( img, city->m_longitude.DoubleValue() - iconSize/2, city->m_latitude.DoubleValue() - iconSize/2, iconSize, iconSize, col );

                    if( city->m_numLACMInQueue )
                    {
                        col.m_a = 100;
                        char caption[128];
                        strcpy( caption, LANGUAGEPHRASE("dialog_mapr_lacm_in_queue") );
                        LPREPLACEINTEGERFLAG( 'N', city->m_numLACMInQueue, caption );
                        g_renderer2d->TextCentreSimple( city->m_longitude.DoubleValue(), yPos, col, inboundTextSize, caption );
                        yPos += 0.5f;
                    }
                    if( city->m_numLACMInFlight )
                    {
                        col.m_a = 255;
                        char caption[128];
                        strcpy( caption, LANGUAGEPHRASE("dialog_mapr_lacm_in_flight") );
                        LPREPLACEINTEGERFLAG( 'N', city->m_numLACMInFlight, caption );
                        g_renderer2d->TextCentreSimple( city->m_longitude.DoubleValue(), yPos, col, inboundTextSize, caption );
                    }
                }
            }            
        }
    }
        
    
    //
    // City / country names

    bool showCityNames      = g_preferences->GetInt( PREFS_GRAPHICS_CITYNAMES );
    bool showCountryNames   = g_preferences->GetInt( PREFS_GRAPHICS_COUNTRYNAMES );

    if( showCityNames || showCountryNames )
    {
        Colour cityColour       (120,180,255,255);
        Colour countryColour    (150,255,150,0);

        cityColour.m_a      = 200.0f * (1.0f - sqrtf(m_zoomFactor));
        countryColour.m_a   = 200.0f * (1.0f - sqrtf(m_zoomFactor));

        for( int i = 0; i < g_app->GetWorld()->m_cities.Size(); ++i )
        {
            City *city = g_app->GetWorld()->m_cities.GetData(i);

            if( city->m_capital || city->m_teamId != -1 )
            {            
                if( IsOnScreen( city->m_longitude.DoubleValue(), city->m_latitude.DoubleValue() ) )
                {
                    float textSize = sqrtf( sqrtf((float) city->m_population) ) / 25.0f;

                    if( showCityNames )
                    {                                                  
                        textSize *= textSize * 0.5;
                        textSize *= sqrtf( sqrtf( m_zoomFactor ) ) * 0.8f;

                        g_renderer2d->TextCentreSimple( city->m_longitude.DoubleValue(), city->m_latitude.DoubleValue(), cityColour, textSize, LANGUAGEPHRASEADDITIONAL(city->m_name) );
                    }                

                    if( showCountryNames && city->m_capital )
                    {
                        float countrySize = textSize;
                        g_renderer2d->TextCentreSimple( city->m_longitude.DoubleValue(), city->m_latitude.DoubleValue()-textSize, countryColour, countrySize, LANGUAGEPHRASEADDITIONAL(city->m_country) );
                    }
                }
            }
        }
    }

	g_renderer->SetFont();

    END_PROFILE( "Cities" );
}

void MapRenderer::RenderPopulationDensity()
{
    START_PROFILE( "Population Density" );

    g_renderer->SetBlendMode( Renderer::BlendModeAdditive );
    
    for( int i = 0; i < g_app->GetWorld()->m_cities.Size(); ++i )
    {
        if( g_app->GetWorld()->m_cities.ValidIndex(i) )
        {
            City *city = g_app->GetWorld()->m_cities.GetData(i);

            if( city->m_teamId != -1 )
            {            
                float size = sqrtf( sqrtf((float) city->m_population) ) / 2.0f;

                Colour col = g_app->GetWorld()->GetTeam(city->m_teamId)->GetTeamColour();
                col.m_a = 255.0f * min( 1.0f, city->m_population / 10000000.0f );
                                    
                g_renderer2d->StaticSprite( g_resource->GetImage( "graphics/population.bmp" ),
                                                  city->m_longitude.DoubleValue()-size/2, city->m_latitude.DoubleValue()-size/2,
                                                  size, size, col );
            }
        }
    }

    END_PROFILE( "Population Density" );
}


void MapRenderer::RenderRadar()
{
    START_PROFILE( "Radar" );
    
    g_renderer->SetBlendMode( Renderer::BlendModeAdditive );
    g_renderer->SetDepthBuffer( true, true );

    //
    // begin radar micro batching (own radar)

    g_renderer2d->BeginCircleBatch();
    g_renderer2d->BeginCircleFillBatch();
    
    //
    // Render our radar

    RenderRadar( false, false );  // Own filled circles
    RenderRadar( false, true );   // Own outlined circles

    //
    // end it

    g_renderer2d->EndCircleFillBatch();
    g_renderer2d->EndCircleBatch();
    
    //
    // check if we need allied radar

    int myTeamId = g_app->GetWorld()->m_myTeamId;
    bool sharingRadar = false;
    for( int i = 0; i < g_app->GetWorld()->m_teams.Size(); ++i )
    {
        Team *team = g_app->GetWorld()->m_teams[i];
        if( team->m_teamId != myTeamId &&
            myTeamId != -1 &&
            team->m_sharingRadar[myTeamId] )
        {
            sharingRadar = true;
            break;
        }
    }

    //
    // begin allied radar micro batch

    g_renderer2d->BeginCircleBatch();
    g_renderer2d->BeginCircleFillBatch();

    if( sharingRadar )
    {
        RenderRadar( true, false );  // Allied filled circles
        RenderRadar( true, true );   // Allied outlined circles
    }

    //
    // end it

    g_renderer2d->EndCircleFillBatch();
    g_renderer2d->EndCircleBatch();
    
    //
    // darken the whole world not covered by radar (uses depth buffer as mask)
    // cannot use batching here as im having issues with depth and blend modes 
    // when flushing

    g_renderer->SetBlendMode( Renderer::BlendModeNormal );        
    g_renderer2d->RectFill( -180, -100, 360, 200, Colour(0,0,0,150), true );
    g_renderer->SetDepthBuffer( false, false );
    g_renderer->SetBlendMode( Renderer::BlendModeAdditive );      
    
    END_PROFILE( "Radar" );
}

void MapRenderer::RenderRadar( bool _allies, bool _outlined )
{
    int myTeamId = g_app->GetWorld()->m_myTeamId;
    float timeFactor = g_predictionTime * g_app->GetWorld()->GetTimeScaleFactor().DoubleValue();

    for( int i = 0; i < g_app->GetWorld()->m_objects.Size(); ++i )
    {
        if( g_app->GetWorld()->m_objects.ValidIndex(i) )
        {
            WorldObject *wobj = g_app->GetWorld()->m_objects[i];            
            Team *team = g_app->GetWorld()->GetTeam(wobj->m_teamId);

            if( (_allies && team->m_sharingRadar[myTeamId]) ||
                (!_allies && wobj->m_teamId == myTeamId) )
            {
                float size = wobj->GetRadarRange().DoubleValue();      
                if( size > 0.0f )
                {
                    float predictedLongitude = wobj->m_longitude.DoubleValue() + wobj->m_vel.x.DoubleValue() * timeFactor;
                    float predictedLatitude = wobj->m_latitude.DoubleValue() + wobj->m_vel.y.DoubleValue() * timeFactor;
                    
                    //
                    // Important!
                    // i changed the number of segments from 50 to 30  for radar circles
                    // this improves performance by a quite a bit, but incase find a 
                    // better way to do this ill leave this here
                    
                    if( _outlined )
                    {
                        Colour col(150,150,255,255);
                        float lineWidth = 3.0f;
                        if( wobj->m_teamId != myTeamId )
                        {
                            col.m_a = 50;
                            lineWidth = 1.0f;
                        }
                        
                        g_renderer2d->Circle( predictedLongitude-360, predictedLatitude, size+0.1f, 30, col, lineWidth );
                        g_renderer2d->Circle( predictedLongitude,     predictedLatitude, size+0.1f, 30, col, lineWidth );
                        g_renderer2d->Circle( predictedLongitude+360, predictedLatitude, size+0.1f, 30, col, lineWidth );
                    }
                    else
                    {
                        Colour col(150,150,200,30);
                        if( wobj->m_teamId != myTeamId )
                        {
                            col.m_a = 0;
                        }
 
                        g_renderer2d->CircleFill( predictedLongitude-360, predictedLatitude, size, 30, col );
                        g_renderer2d->CircleFill( predictedLongitude,     predictedLatitude, size, 30, col );
                        g_renderer2d->CircleFill( predictedLongitude+360, predictedLatitude, size, 30, col );
                    }
                }
            }
        }
    }
}

void MapRenderer::RenderNodes()
{
    if( g_preferences->GetInt( "RenderNodes" ) != 1 )
    {
        return;
    }


    //
    // Placement points
     
    for( int i = 0; i < g_app->GetWorld()->m_aiPlacementPoints.Size(); ++i )
    {
        Vector3<Fixed> *thisPoint = g_app->GetWorld()->m_aiPlacementPoints[i];

        Colour col(0,255,0,55);

        g_renderer2d->CircleFill( thisPoint->x.DoubleValue(), thisPoint->y.DoubleValue(), 0.5, 20, col );
    }


    //
    // Target points

    for( int i = 0; i < g_app->GetWorld()->m_aiTargetPoints.Size(); ++i )
    {
        Vector3<Fixed> *thisPoint = g_app->GetWorld()->m_aiTargetPoints[i];

        Fixed attackRange = 60;

        bool inRange = false;
        for( int j = 0; j < World::NumTerritories; ++j )
        {
            int owner = g_app->GetWorld()->GetTerritoryOwner(j);
            if( owner != -1 &&
                g_app->GetWorld()->m_myTeamId != -1 &&
                !g_app->GetWorld()->IsFriend( owner, g_app->GetWorld()->m_myTeamId ) )
            {
                Fixed popCenterLong = g_app->GetWorld()->m_populationCenter[j].x;
                Fixed popCenterLat = g_app->GetWorld()->m_populationCenter[j].y;
                if( g_app->GetWorld()->GetDistance( thisPoint->x, thisPoint->y, popCenterLong, popCenterLat ) < attackRange )
                {
                    inRange = true;
                    break;
                }
            }
        }

        Colour col(255,0,0,255);
        
        if( inRange )
        {
            g_renderer2d->CircleFill( thisPoint->x.DoubleValue(), thisPoint->y.DoubleValue(), 1, 20, col );
        }
        else
        {
            g_renderer2d->Circle( thisPoint->x.DoubleValue(), thisPoint->y.DoubleValue(), 1, 20, col );
        }
    }


    //
    // Travel node graph

    g_renderer->SetFont( NULL, true );

    for( int i = 0; i < g_app->GetWorld()->m_nodes.Size(); ++i )
    {
        if( g_app->GetWorld()->m_nodes.ValidIndex(i) )
        {
            Node *node = g_app->GetWorld()->m_nodes[i];
            g_renderer2d->CircleFill( node->m_longitude.DoubleValue(), node->m_latitude.DoubleValue(), 1.0f, 20, Colour(255,255,255,150) );
            char num[16];
            sprintf(num, "%d", i );

            g_renderer2d->Text( node->m_longitude.DoubleValue() + 0.5f, 
                              node->m_latitude.DoubleValue() + 0.5f, White, 2.0f, num );
            
            for( int j = 0; j < node->m_routeTable.Size(); ++j )
            {
                Node *nextNode = g_app->GetWorld()->m_nodes[node->m_routeTable[j]->m_nextNodeId];
                float lineWidth = 1.5f;

                g_renderer2d->Line(node->m_longitude.DoubleValue(), 
                                 node->m_latitude.DoubleValue(), 
                                 nextNode->m_longitude.DoubleValue(), 
                                 nextNode->m_latitude.DoubleValue(), 
                                 Colour(255,255,255,35), lineWidth );
            }
        }
    }

    g_renderer->SetFont();
}


void MapRenderer::UpdateCameraControl( float longitude, float latitude )
{
    //
    // Handle zooming

    m_totalZoom += g_inputManager->m_mouseVelZ * g_preferences->GetFloat(PREFS_INTERFACE_ZOOM_SPEED, 1.0);

    Clamp( m_totalZoom, 0.0f, 30.0f );
    float zoomFactor = pow(0.9f, m_totalZoom );

    if( zoomFactor < 0.05f ) zoomFactor = 0.05f;
    if( zoomFactor > 1.0f ) zoomFactor = 1.0f;    
	
    if( fabs(zoomFactor - m_zoomFactor) > 0.01f )
    {        
        float factor1 = g_advanceTime * 5.0f;
        float factor2 = 1.0f - factor1;
        m_zoomFactor = zoomFactor * factor1 + m_zoomFactor * factor2;

        float factor3 = g_advanceTime * 0.5f;
        float factor4 = 1.0f - factor3;

        int x = longitude;
        int y = latitude;

        m_middleX = x * factor3 + m_middleX * factor4;
        m_middleY = y * factor3 + m_middleY * factor4;        
    }

    if( longitude < -180.0f ) longitude += 360.0f;
    if( longitude >180.0f ) longitude -= 360.0f;


    //
    // Quake key support

    float left, right, top, bottom, width, height;
    GetWindowBounds( &left, &right, &top, &bottom );
    width = right - left;
    height = bottom - top;

    //MoveCam();

    bool ignoreKeys = g_app->GetInterface()->UsingChatWindow();
    if( !m_lockCamControl )
    {
        float oldX = m_middleX;
        float oldY = m_middleY;
        float scrollSpeed = 150.0f * m_zoomFactor * g_advanceTime;
        int edgeSize = 10;

        // only scroll if we're the foreground app, and the user allows us to
        bool sideScrolling = g_inputManager->m_windowHasFocus && g_preferences->GetInt( PREFS_INTERFACE_SIDESCROLL );
        int keyboardLayout = g_preferences->GetInt( PREFS_INTERFACE_KEYBOARDLAYOUT );

        if( m_draggingCamera ) sideScrolling = false;
		int KeyQ = KEY_Q, KeyW = KEY_W, KeyE = KEY_E, KeyA = KEY_A, KeyS = KEY_S, KeyD = KEY_D;
		
		switch ( keyboardLayout) {
		case 2:			// AZERTY
			KeyQ = KEY_A;
			KeyW = KEY_Z;
			KeyA = KEY_Q;
			break;
		case 3:			// QZERTY
			KeyW = KEY_Z;
			break;
		case 4:			// DVORAK
			KeyQ = KEY_QUOTE;
			KeyW = KEY_COMMA;
			KeyE = KEY_STOP;
			KeyS = KEY_O;
			KeyD = KEY_E;
			break;
		default:		// QWERTY/QWERTZ (1) or invaild pref - do nothing
			break;
		}
        if( ((g_keys[KeyA] || g_keys[KEY_LEFT]) && !ignoreKeys)   || (g_inputManager->m_mouseX <= edgeSize && sideScrolling ) )			   m_middleX -= scrollSpeed;
        if( ((g_keys[KeyD] || g_keys[KEY_RIGHT]) && !ignoreKeys)  || (g_inputManager->m_mouseX >= m_pixelW-edgeSize  && sideScrolling ))   m_middleX += scrollSpeed;
        if( ((g_keys[KeyW] || g_keys[KEY_UP]) && !ignoreKeys)     || (g_inputManager->m_mouseY <= edgeSize && sideScrolling ))             m_middleY += scrollSpeed;
        if( ((g_keys[KeyS] || g_keys[KEY_DOWN]) && !ignoreKeys)   || (g_inputManager->m_mouseY >= m_pixelH-edgeSize && sideScrolling ) )   m_middleY -= scrollSpeed;

		if( g_keys[KeyE] && !ignoreKeys) m_totalZoom -= 20 * g_advanceTime;
		if( g_keys[KeyQ] && !ignoreKeys) m_totalZoom += 20 * g_advanceTime;

        if( m_middleX == oldX && 
            m_middleY == oldY )
        {
            m_cameraIdleTime += SERVER_ADVANCE_PERIOD.DoubleValue();
        }
        else
        {
            m_cameraIdleTime = 0.0f;
        }

        if( g_preferences->GetInt( PREFS_INTERFACE_CAMDRAGGING ) == 1 )
        {
            DragCamera();
        }
        else
        {
            if( !g_app->MousePointerIsVisible() )
            {
                g_app->SetMousePointerVisible ( true );
            }
        }

    }


    float maxWidth = 360.0f;    
    float aspect = (float) m_pixelH / (float) m_pixelW;
    float maxHeight = (360.0f * aspect);

    if( m_middleX > maxWidth/2 ) m_middleX = -maxWidth/2 + m_middleX-maxWidth/2;
    if( m_middleX < -maxWidth/2 ) m_middleX = 180 - m_middleX/-maxWidth/2;
    if( m_middleY + height/2 < -maxHeight/2 ) m_middleY = -maxHeight/2 - height/2;
    if( m_middleY - height/2 > maxHeight/2 ) m_middleY = maxHeight/2 + height/2;
}


int MapRenderer::GetNearestObjectToMouse( float _mouseX, float _mouseY )
{
    if( _mouseX > 180 ) _mouseX = -180 + ( _mouseX - 180 );
    if( _mouseX < -180 ) _mouseX = 180 + ( _mouseX + 180 );

    int underMouseId = -1;

    Fixed nearest = 5;
    if( g_app->GetWorldRenderer()->GetCurrentSelectionId() != -1 )
    {
        nearest = 2;
    }

    if( g_app->GetMapRenderer()->GetZoomFactor() <= 0.25f )
    {
        nearest *= Fixed::FromDouble(g_app->GetMapRenderer()->GetZoomFactor() * 4);
    }

    Fixed mouseLong = Fixed::FromDouble(_mouseX);
    Fixed mouseLat = Fixed::FromDouble(_mouseY);
    Fixed maxConsiderSqd = Fixed(25) * Fixed(25);  // Skip objects farther than 25 units

    //
    // Find the nearest object to the Mouse

    for( int i = 0; i < g_app->GetWorld()->m_objects.Size(); ++i )
    {
        if( g_app->GetWorld()->m_objects.ValidIndex(i) )
        {
            WorldObject *wobj = g_app->GetWorld()->m_objects[i];
            Fixed distSqd = g_app->GetWorld()->GetDistanceSqd( wobj->m_longitude, wobj->m_latitude, mouseLong, mouseLat );
            if( distSqd > maxConsiderSqd )
                continue;
            if( g_app->GetWorld()->m_myTeamId == -1 ||
                wobj->m_visible[g_app->GetWorld()->m_myTeamId] ||
                wobj->m_lastSeenTime[g_app->GetWorld()->m_myTeamId] > 0 ||
                g_app->GetGame()->m_winner != -1 )
            {
                Fixed distance = g_app->GetWorld()->GetDistance( wobj->m_longitude, wobj->m_latitude, mouseLong, mouseLat );
                if( distance < nearest )
                {
                    underMouseId = wobj->m_objectId;
                    nearest = distance;
                }
            }
        }
    }

    //
    // Any cities nearer?

    for( int i = 0; i < g_app->GetWorld()->m_cities.Size(); ++i )
    {
        City *city = g_app->GetWorld()->m_cities[i];
        Fixed distSqd = g_app->GetWorld()->GetDistanceSqd( city->m_longitude, city->m_latitude, mouseLong, mouseLat );
        if( distSqd > maxConsiderSqd )
            continue;
        Fixed distance = g_app->GetWorld()->GetDistance( city->m_longitude, city->m_latitude, mouseLong, mouseLat );
        if( distance < nearest )
        {
            underMouseId = city->m_objectId;
            nearest = distance;
        }
    }

    return underMouseId;
}


void MapRenderer::HandleSelectObject( int _underMouseId )
{
    bool landed = false;
    bool selected = false;

    WorldObject *undermouse = g_app->GetWorld()->GetWorldObject(_underMouseId);
    if( !undermouse || undermouse->m_teamId != g_app->GetWorld()->m_myTeamId )
    {
        return;
    }

    WorldObject *selection = g_app->GetWorld()->GetWorldObject(g_app->GetWorldRenderer()->GetCurrentSelectionId());

    //
    // If clicking on airbase/carrier with aircraft selected, request landing

    if( selection && 
        selection->m_teamId == g_app->GetWorld()->m_myTeamId )
    {
        bool selectionLandable = selection->IsAircraft();

        bool underMouseLandable = undermouse->IsAircraftLauncher();

        if( selectionLandable && underMouseLandable )
        {
            g_app->GetClientToServer()->RequestSpecialAction( g_app->GetWorldRenderer()->GetCurrentSelectionId(), _underMouseId, World::SpecialActionLandingAircraft );
            g_app->GetWorldRenderer()->CreateAnimation(  WorldRenderer::AnimationTypeActionMarker, selection->m_objectId,
                             undermouse->m_longitude.DoubleValue(), undermouse->m_latitude.DoubleValue() );
            g_app->GetWorldRenderer()->SetCurrentSelectionId(-1);
            UpdateSelectionAnimation(-1);
            landed = true;
        }
    }

    if( !landed )
    {
        bool selectable = undermouse->IsActionable() ||
                          undermouse->IsMovingObject();

        if( selectable && !undermouse->IsBallisticMissileClass() )
        {
            if( g_keys[KEY_CONTROL] && g_keys[KEY_SHIFT] )
            {
                g_app->GetWorldRenderer()->RemoveFromSelection(_underMouseId);
                UpdateSelectionAnimation(g_app->GetWorldRenderer()->GetCurrentSelectionId());
                selected = true;
            }
            else if( g_keys[KEY_CONTROL] )
            {
                int unitType = undermouse->m_type;
                int myTeam = g_app->GetWorld()->m_myTeamId;
                g_app->GetWorldRenderer()->ClearSelection();
                for( int i = 0; i < g_app->GetWorld()->m_objects.Size(); ++i )
                {
                    if( !g_app->GetWorld()->m_objects.ValidIndex(i) ) continue;
                    WorldObject *obj = g_app->GetWorld()->m_objects[i];
                    if( !obj || obj->m_teamId != myTeam || obj->m_type != unitType ) continue;
                    if( obj->IsBallisticMissileClass() ) continue;
                    if( !obj->IsActionable() && !obj->IsMovingObject() ) continue;
                    if( !IsOnScreen( obj->m_longitude.DoubleValue(), obj->m_latitude.DoubleValue() ) ) continue;
                    g_app->GetWorldRenderer()->AddToSelection( obj->m_objectId );
                }
                g_app->GetWorldRenderer()->SetPrimarySelectionId( _underMouseId );
                UpdateSelectionAnimation( _underMouseId );
                selected = true;
            }
            else if( g_keys[KEY_SHIFT] )
            {
                g_app->GetWorldRenderer()->AddToSelection(_underMouseId);
                UpdateSelectionAnimation(_underMouseId);
                selected = true;
            }
            else
            {
                g_app->GetWorldRenderer()->SetCurrentSelectionId(_underMouseId);
                UpdateSelectionAnimation(_underMouseId);
                selected = true;
            }
        }
    }

    if( !landed && !selected )
    {
        g_app->GetWorldRenderer()->SetCurrentSelectionId(-1);
        UpdateSelectionAnimation(-1);
    }
}


void MapRenderer::HandleClickStateMenu()
{
    WorldObject *highlight = g_app->GetWorld()->GetWorldObject(g_app->GetWorldRenderer()->GetCurrentHighlightId());
    if( highlight )
    {
        if( g_keys[KEY_SHIFT] && m_stateApplyToSelectionSameType && g_app->GetWorldRenderer()->GetSelectionCount() > 0 )
        {
            // Shift+click: apply to all selected units of same type (exact subunit)
            int targetType = highlight->m_type;
            bool anySuccess = false;
            bool anyFailure = false;
            for( int i = 0; i < g_app->GetWorldRenderer()->GetSelectionCount(); ++i )
            {
                int thisObjId = g_app->GetWorldRenderer()->GetSelectedId( i );
                WorldObject *thisObj = g_app->GetWorld()->GetWorldObject( thisObjId );
                if( thisObj && thisObj->m_type == targetType )
                {
                    if( m_currentStateId == CLEARQUEUE_STATEID )
                    {
                        g_app->GetClientToServer()->RequestClearActionQueue( thisObj->m_objectId );
                        anySuccess = true;
                    }
                    else
                    {
                        if( thisObj->CanSetState(m_currentStateId) )
                        {
                            g_app->GetClientToServer()->RequestStateChange( thisObj->m_objectId, m_currentStateId );
                            anySuccess = true;
                        }
                        else
                        {
                            anyFailure = true;
                        }
                    }
                }
            }
            if( anySuccess )
                g_soundSystem->TriggerEvent( "Interface", "SelectObjectState" );
            if( anyFailure )
                g_soundSystem->TriggerEvent( "Interface", "Error" );
        }
        else if( g_keys[KEY_SHIFT] &&
            highlight->m_fleetId != -1 )
        {
            //
            // SHIFT pressed - set for all units in fleet

            Fleet *fleet = g_app->GetWorld()->GetTeam(highlight->m_teamId)->GetFleet(highlight->m_fleetId);
            if( fleet )
            {
                for( int i = 0; i < fleet->m_fleetMembers.Size(); ++i )
                {
                    int thisObjId = fleet->m_fleetMembers[i];
                    WorldObject *thisObj = g_app->GetWorld()->GetWorldObject(thisObjId);
                    if( thisObj && 
                        thisObj->m_type == highlight->m_type )
                    {
                        if( m_currentStateId == CLEARQUEUE_STATEID )
                        {
                            g_app->GetClientToServer()->RequestClearActionQueue( thisObj->m_objectId );
                            g_soundSystem->TriggerEvent( "Interface", "SelectObjectState" );           
                        }
                        else
                        {
                            if( thisObj->CanSetState(m_currentStateId) )
                            {
                                g_app->GetClientToServer()->RequestStateChange( thisObj->m_objectId, m_currentStateId );
                                g_soundSystem->TriggerEvent( "Interface", "SelectObjectState" );           
                            }
                            else
                            {
                                g_soundSystem->TriggerEvent( "Interface", "Error" );
                            }
                        }                        
                    }
                }
            }
        }
        else
        {
            if( m_currentStateId == CLEARQUEUE_STATEID )
            {
                //
                // Clear action queue clicked

                g_app->GetClientToServer()->RequestClearActionQueue( g_app->GetWorldRenderer()->GetCurrentHighlightId() );
                g_soundSystem->TriggerEvent( "Interface", "SelectObjectState" );           
            }
            else
            {
                g_app->GetClientToServer()->RequestStateChange( g_app->GetWorldRenderer()->GetCurrentHighlightId(), m_currentStateId );

                //
                // Select the object if it can spawn/do stuff
                
                if( highlight )
                {
                    if( highlight->m_currentState == m_currentStateId ||
                        highlight->CanSetState(m_currentStateId) )
                    {                         
                        g_soundSystem->TriggerEvent( "Interface", "SelectObjectState" );           

                        if( highlight->m_states[m_currentStateId]->m_isActionable )
                        {
                            int highlightId = g_app->GetWorldRenderer()->GetCurrentHighlightId();
                            g_app->GetWorldRenderer()->SetCurrentSelectionId( highlightId );
                            UpdateSelectionAnimation( highlightId );                             
                        }
                    }
                    else
                    {
                        g_soundSystem->TriggerEvent( "Interface", "Error" );
                    }
                }
            }
        }
    }

    m_stateRenderTime = 0.0f;
    m_stateObjectId = -1;
    m_stateApplyToSelectionSameType = false;
}


static bool SelectionHasNukingUnit()
{
    int selCount = g_app->GetWorldRenderer()->GetSelectionCount();
    for( int s = 0; s < selCount; ++s )
    {
        int recId = g_app->GetWorldRenderer()->GetSelectedId( s );
        WorldObject *obj = g_app->GetWorld()->GetWorldObject( recId );
        if( obj && obj->UsingNukes() )
            return true;
    }
    return false;
}


static bool SelectionHasLaunchableUnit()
{
    int selCount = g_app->GetWorldRenderer()->GetSelectionCount();
    for( int s = 0; s < selCount; ++s )
    {
        int recId = g_app->GetWorldRenderer()->GetSelectedId( s );
        WorldObject *obj = g_app->GetWorld()->GetWorldObject( recId );
        if( obj && ( obj->CanLaunchFighter() || obj->CanLaunchBomber() ) )
            return true;
    }
    return false;
}


static bool SelectionHasStandbyNukeUnit()
{
    int selCount = g_app->GetWorldRenderer()->GetSelectionCount();
    for( int s = 0; s < selCount; ++s )
    {
        int recId = g_app->GetWorldRenderer()->GetSelectedId( s );
        WorldObject *obj = g_app->GetWorld()->GetWorldObject( recId );
        if( !obj ) continue;
        if( obj->m_type == WorldObject::TypeSub && obj->m_currentState == 2 ) return true;
        if( obj->m_type == WorldObject::TypeSilo && obj->m_currentState == 0 ) return true;
        if( obj->m_type == WorldObject::TypeBomber && obj->m_currentState == 0 ) return true;
    }
    return false;
}


static bool SelectionHasMovingUnit()
{
    int selCount = g_app->GetWorldRenderer()->GetSelectionCount();
    for( int s = 0; s < selCount; ++s )
    {
        int recId = g_app->GetWorldRenderer()->GetSelectedId( s );
        WorldObject *obj = g_app->GetWorld()->GetWorldObject( recId );
        if( obj && obj->IsMovingObject() )
            return true;
    }
    return false;
}


static int ResolveNukeTargetForStatic( Fixed longitude, Fixed latitude )
{
    // Find static target (city, building) at position. Nukes target static only.
    World *world = g_app->GetWorld();
    Fixed nearestSqd = Fixed( 25 );  // 5 units threshold
    int resultId = -1;

    for( int i = 0; i < world->m_cities.Size(); ++i )
    {
        if( !world->m_cities.ValidIndex(i) ) continue;
        City *city = world->m_cities[i];
        Fixed distSqd = world->GetDistanceSqd( city->m_longitude, city->m_latitude, longitude, latitude );
        if( distSqd < nearestSqd )
        {
            nearestSqd = distSqd;
            resultId = city->m_objectId;
        }
    }

    for( int i = 0; i < world->m_objects.Size(); ++i )
    {
        if( !world->m_objects.ValidIndex(i) ) continue;
        WorldObject *obj = world->m_objects[i];
        if( !obj || obj->IsMovingObject() ) continue;
        if( !obj->IsSiloClass() &&
            !obj->IsRadarClass() &&
            !obj->IsAirbaseClass() )
            continue;
        Fixed distSqd = world->GetDistanceSqd( obj->m_longitude, obj->m_latitude, longitude, latitude );
        if( distSqd < nearestSqd )
        {
            nearestSqd = distSqd;
            resultId = obj->m_objectId;
        }
    }

    return resultId;
}


void MapRenderer::HandleObjectAction( float _mouseX, float _mouseY, int underMouseId )
{
    WorldObject *underMouse = g_app->GetWorld()->GetWorldObject(underMouseId);

    int selCount = g_app->GetWorldRenderer()->GetSelectionCount();
    if( selCount == 0 )
        return;

    Fixed targetLong;
    Fixed targetLat;
    if( underMouse )
    {
        targetLong = underMouse->m_longitude;
        targetLat = underMouse->m_latitude;
    }
    else
    {
        targetLong = Fixed::FromDouble( _mouseX );
        targetLat = Fixed::FromDouble( _mouseY );
    }

    bool anyOrderGiven = false;
    int lastQueuedId = -1;

    for( int si = 0; si < selCount; ++si )
    {
        int recId = g_app->GetWorldRenderer()->GetSelectedId( si );
        WorldObject *obj = g_app->GetWorld()->GetWorldObject( recId );
        if( !obj ) continue;

        int numTimesRemaining = obj->m_states[obj->m_currentState]->m_numTimesPermitted;
        if( numTimesRemaining > -1 ) numTimesRemaining -= obj->m_actionQueue.Size();
        if( numTimesRemaining == 0 && !obj->SetWaypointOnAction() ) continue;

        bool canAction = true;
        if( !underMouse )
        {
            if( obj->IsSubmarine() && obj->UsingNukes() )
            {
                Fixed distSqd = g_app->GetWorld()->GetDistanceSqd( obj->m_longitude, obj->m_latitude, targetLong, targetLat );
                canAction = ( distSqd < obj->GetActionRangeSqd() );
            }
            else if( obj->UsingNukes() )
            {
                int moveResult = obj->IsValidMovementTarget( targetLong, targetLat );
                canAction = ( moveResult > WorldObject::TargetTypeInvalid ||
                             moveResult == WorldObject::TargetTypeOutOfRange );
            }
            else if( obj->IsSubmarine() )
            {
                canAction = ( obj->m_currentState == 2 );
            }
            else
            {
                if( obj->IsBattleShipClass() )
                    canAction = false;
                if( obj->IsAircraftLauncher() )
                {
                    if( obj->IsValidMovementTarget( targetLong, targetLat ) < WorldObject::TargetTypeValid )
                        canAction = false;
                }
            }
        }
        else
        {
            int combatResult = obj->IsValidCombatTarget( underMouseId );
            canAction = ( combatResult > WorldObject::TargetTypeInvalid ||
                         combatResult == WorldObject::TargetTypeOutOfRange );
        }

        if( !canAction ) continue;

        if( obj->SetWaypointOnAction() )
            g_app->GetClientToServer()->RequestSetWaypoint( recId, targetLong, targetLat );
        g_app->GetClientToServer()->RequestAction( recId, underMouseId, targetLong, targetLat );

        int animid = g_app->GetWorldRenderer()->CreateAnimation( g_app->GetWorldRenderer()->AnimationTypeActionMarker, recId, targetLong.DoubleValue(), targetLat.DoubleValue() );
        ActionMarker *action = (ActionMarker *) g_app->GetWorldRenderer()->GetAnimations()[animid];

        if( !underMouse )
        {
            action->m_targetType = obj->IsValidMovementTarget( targetLong, targetLat );
            action->m_combatTarget = false;
        }
        else
        {
            action->m_targetType = obj->IsValidCombatTarget( underMouseId );
            action->m_combatTarget = ( action->m_targetType > WorldObject::TargetTypeInvalid );
            if( action->m_targetType == WorldObject::TargetTypeInvalid &&
                obj->IsAircraftLauncher() &&
                obj->m_currentState == 0 )
            {
                action->m_targetType = WorldObject::TargetTypeLaunchFighter;
                action->m_combatTarget = false;
            }
        }

        if( action->m_targetType >= WorldObject::TargetTypeValid )
            g_soundSystem->TriggerEvent( "Interface", "SetCombatTarget" );

        anyOrderGiven = true;
        if( obj->IsActionQueueable() && ( numTimesRemaining == -1 || numTimesRemaining > 1 ) )
            lastQueuedId = recId;
    }

    if( underMouse )
        underMouse->m_nukeCountTimer = GetHighResTime() + 0.1f;

    if( !anyOrderGiven )
    {
        g_soundSystem->TriggerEvent( "Interface", "Error" );
        m_tooltipTimer = 0.0f;
    }

    if( lastQueuedId != -1 )
    {
        // Keep full selection so user can keep queueing for the group
        g_app->GetWorldRenderer()->SetPrimarySelectionId( lastQueuedId );
        UpdateSelectionAnimation( lastQueuedId );
    }
    else
    {
        g_app->GetWorldRenderer()->ClearSelection();
        UpdateSelectionAnimation(-1);
    }
}


void MapRenderer::HandleSetWaypoint( float _mouseX, float _mouseY, bool _isDoubleClick )
{
    Fixed mouseX = Fixed::FromDouble(_mouseX);
    Fixed mouseY = Fixed::FromDouble(_mouseY);
    bool anySuccess = false;
    LList<int> fleetsMoved;

    int selCount = g_app->GetWorldRenderer()->GetSelectionCount();
    for( int s = 0; s < selCount; ++s )
    {
        int recId = g_app->GetWorldRenderer()->GetSelectedId( s );
        WorldObject *wobj = g_app->GetWorld()->GetWorldObject( recId );
        if( !wobj || !wobj->IsMovingObject() )
            continue;

        MovingObject *obj = (MovingObject *) wobj;

        if( obj->m_fleetId != -1 )
        {
            //
            // Moving a fleet - only request once per fleet
            bool alreadyMoved = false;
            for( int f = 0; f < fleetsMoved.Size(); ++f )
            {
                if( fleetsMoved.GetData(f) == obj->m_fleetId ) { alreadyMoved = true; break; }
            }
            if( alreadyMoved ) continue;

            Fleet *fleet = g_app->GetWorld()->GetTeam( obj->m_teamId )->GetFleet( obj->m_fleetId );
            if( fleet &&
                fleet->IsValidFleetPosition( mouseX, mouseY ) &&
                g_app->GetWorld()->GetClosestNode( mouseX, mouseY ) != -1 )
            {
                g_app->GetClientToServer()->RequestFleetMovement( obj->m_teamId, obj->m_fleetId, mouseX, mouseY, _isDoubleClick );
                fleetsMoved.PutData( obj->m_fleetId );
                anySuccess = true;

                for( int i = 0; i < fleet->m_fleetMembers.Size(); ++i )
                {
                    WorldObject *thisShip = g_app->GetWorld()->GetWorldObject( fleet->m_fleetMembers[i] );
                    if( thisShip )
                    {
                        Fixed offsetX = 0, offsetY = 0;
                        fleet->GetFormationPosition( fleet->m_fleetMembers.Size(), i, &offsetX, &offsetY );
                        int index = g_app->GetWorldRenderer()->CreateAnimation( g_app->GetWorldRenderer()->AnimationTypeActionMarker, thisShip->m_objectId,
                                 _mouseX+offsetX.DoubleValue(), _mouseY+offsetY.DoubleValue() );
                        ActionMarker *marker = (ActionMarker *)g_app->GetWorldRenderer()->GetAnimations()[index];
                        marker->m_targetType = WorldObject::TargetTypeValid;
                    }
                }
                g_soundSystem->TriggerEvent( "Interface", "SetMovementTarget" );
            }
        }
        else
        {
            g_app->GetClientToServer()->RequestSetWaypoint( recId, mouseX, mouseY, _isDoubleClick );
            int index = g_app->GetWorldRenderer()->CreateAnimation( g_app->GetWorldRenderer()->AnimationTypeActionMarker, recId, _mouseX, _mouseY );
            ActionMarker *marker = (ActionMarker *)g_app->GetWorldRenderer()->GetAnimations()[index];
            marker->m_targetType = WorldObject::TargetTypeValid;
            g_soundSystem->TriggerEvent( "Interface", "SetMovementTarget" );
            anySuccess = true;
        }
    }

    if( anySuccess )
    {
        g_app->GetWorldRenderer()->ClearSelection();
        UpdateSelectionAnimation(-1);
    }
    else if( selCount == 0 )
    {
        // Fallback for single selection via GetCurrentSelectionId (legacy)
        int recId = g_app->GetWorldRenderer()->GetCurrentSelectionId();
        WorldObject *wobj = g_app->GetWorld()->GetWorldObject( recId );
        if( wobj && wobj->IsMovingObject() )
        {
            MovingObject *obj = (MovingObject *) wobj;
            if( obj->m_fleetId != -1 )
            {
                Fleet *fleet = g_app->GetWorld()->GetTeam( obj->m_teamId )->GetFleet( obj->m_fleetId );
                if( fleet && fleet->IsValidFleetPosition( mouseX, mouseY ) && g_app->GetWorld()->GetClosestNode( mouseX, mouseY ) != -1 )
                {
                    g_app->GetClientToServer()->RequestFleetMovement( obj->m_teamId, obj->m_fleetId, mouseX, mouseY, _isDoubleClick );
                    for( int i = 0; i < fleet->m_fleetMembers.Size(); ++i )
                    {
                        WorldObject *thisShip = g_app->GetWorld()->GetWorldObject( fleet->m_fleetMembers[i] );
                        if( thisShip )
                        {
                            Fixed offsetX = 0, offsetY = 0;
                            fleet->GetFormationPosition( fleet->m_fleetMembers.Size(), i, &offsetX, &offsetY );
                            int index = g_app->GetWorldRenderer()->CreateAnimation( g_app->GetWorldRenderer()->AnimationTypeActionMarker, thisShip->m_objectId,
                                     _mouseX+offsetX.DoubleValue(), _mouseY+offsetY.DoubleValue() );
                            ActionMarker *marker = (ActionMarker *)g_app->GetWorldRenderer()->GetAnimations()[index];
                            marker->m_targetType = WorldObject::TargetTypeValid;
                        }
                    }
                    g_soundSystem->TriggerEvent( "Interface", "SetMovementTarget" );
                    g_app->GetWorldRenderer()->ClearSelection();
                    UpdateSelectionAnimation(-1);
                }
                else
                    g_soundSystem->TriggerEvent( "Interface", "Error" );
            }
            else
            {
                g_app->GetClientToServer()->RequestSetWaypoint( recId, mouseX, mouseY, _isDoubleClick );
                int index = g_app->GetWorldRenderer()->CreateAnimation( g_app->GetWorldRenderer()->AnimationTypeActionMarker, recId, _mouseX, _mouseY );
                ActionMarker *marker = (ActionMarker *)g_app->GetWorldRenderer()->GetAnimations()[index];
                marker->m_targetType = WorldObject::TargetTypeValid;
                g_soundSystem->TriggerEvent( "Interface", "SetMovementTarget" );
                g_app->GetWorldRenderer()->ClearSelection();
                UpdateSelectionAnimation(-1);
            }
        }
    }
    else
    {
        g_soundSystem->TriggerEvent( "Interface", "Error" );
    }
}


void MapRenderer::Update()
{
    UpdateMouseIdleTime();

    // Work out our mouse co-ordinates in long/lat space

    m_pixelX = 0;
    m_pixelY = 0;
    m_pixelW = g_windowManager->WindowW();
    m_pixelH = g_windowManager->WindowH();

    float longitude;
    float latitude;
    ConvertPixelsToAngle( g_inputManager->m_mouseX, g_inputManager->m_mouseY, &longitude, &latitude, true );

    //
    // Camera control

    UpdateCameraControl( longitude, latitude );

    
	//
	// WhiteBoard planning drawing

	if( UpdatePlanning( longitude, latitude ) ) return;


    //
    // Is the mouse over something?

    if( !IsMouseInMapRenderer() ) return;
    
    if( m_highlightTime > 0.0f )    m_highlightTime -= g_advanceTime*2.0f;        
    if( m_highlightTime < 0.0f )    m_highlightTime = 0.0f;
    
    if( m_selectTime > 0.0f )       m_selectTime  -= g_advanceTime;
    if( m_selectTime < 0.0f )       m_selectTime  = 0.0f;

    if( m_deselectTime < 1.0f )     m_deselectTime  += g_advanceTime;    
    if( m_deselectTime > 1.0f )     m_deselectTime = 1.0f;
        

    WorldObject *underMouse = NULL;
    int underMouseId = GetNearestObjectToMouse( longitude, latitude );
    if( underMouseId != -1 ) underMouse = g_app->GetWorld()->GetWorldObject( underMouseId );


#ifndef NON_PLAYABLE
    if( g_app->GetWorld()->GetTimeScaleFactor() > 0 && !m_lockCommands)
    {
        if( g_inputManager->m_lmbClicked )
        {
            bool hasSelection = g_app->GetWorldRenderer()->GetSelectionCount() > 0;
            bool shiftAdd = hasSelection && g_keys[KEY_SHIFT] && !g_keys[KEY_CONTROL];
            bool ctrlShiftRemove = hasSelection && g_keys[KEY_CONTROL] && g_keys[KEY_SHIFT];

            if( !hasSelection )
            {
                m_isDraggingSelect = true;
                m_dragSelectMode = 0;
                m_dragStartLong = Fixed::FromDouble( longitude );
                m_dragStartLat = Fixed::FromDouble( latitude );
                m_dragEndLong = m_dragStartLong;
                m_dragEndLat = m_dragStartLat;
            }
            else if( shiftAdd )
            {
                m_isDraggingSelect = true;
                m_dragSelectMode = 1;
                m_dragStartLong = Fixed::FromDouble( longitude );
                m_dragStartLat = Fixed::FromDouble( latitude );
                m_dragEndLong = m_dragStartLong;
                m_dragEndLat = m_dragStartLat;
            }
            else if( ctrlShiftRemove )
            {
                m_isDraggingSelect = true;
                m_dragSelectMode = 2;
                m_dragStartLong = Fixed::FromDouble( longitude );
                m_dragStartLat = Fixed::FromDouble( latitude );
                m_dragEndLong = m_dragStartLong;
                m_dragEndLat = m_dragStartLat;
            }
        }

        if( g_inputManager->m_lmb )
        {
            if( m_isDraggingSelect )
            {
                m_dragEndLong = Fixed::FromDouble( longitude );
                m_dragEndLat = Fixed::FromDouble( latitude );
            }
        }

        if( g_inputManager->m_lmbUnClicked )                                        // LEFT BUTTON UNCLICKED
        {
            bool consumedByMarquee = false;
            if( m_isDraggingSelect )
            {
                float dx = (m_dragEndLong - m_dragStartLong).DoubleValue();
                float dy = (m_dragEndLat - m_dragStartLat).DoubleValue();
                if( fabsf(dx) > 0.5f || fabsf(dy) > 0.5f )
                {
                    Fixed aLon = m_dragStartLong;
                    Fixed bLon = m_dragEndLong;
                    Fixed aLat = m_dragStartLat;
                    Fixed bLat = m_dragEndLat;
                    if( aLon > bLon ) { Fixed t=aLon; aLon=bLon; bLon=t; }
                    if( aLat > bLat ) { Fixed t=aLat; aLat=bLat; bLat=t; }

                    bool removeMode = ( m_dragSelectMode == 2 );

                    if( m_dragSelectMode == 0 )
                    {
                        g_app->GetWorldRenderer()->ClearSelection();
                    }

                    int myTeam = g_app->GetWorld()->m_myTeamId;
                    for( int i = 0; i < g_app->GetWorld()->m_objects.Size(); ++i )
                    {
                        if( !g_app->GetWorld()->m_objects.ValidIndex(i) ) continue;
                        WorldObject *obj = g_app->GetWorld()->m_objects[i];
                        if( !obj || obj->m_teamId != myTeam ) continue;
                        if( obj->IsBallisticMissileClass() ) continue;
                        if( !obj->IsActionable() && !obj->IsMovingObject() ) continue;

                        Fixed lon = obj->m_longitude;
                        Fixed lat = obj->m_latitude;

                        bool inLat = ( lat >= aLat && lat <= bLat );
                        bool inLon = ( lon >= aLon && lon <= bLon );
                        if( !inLon )
                        {
                            Fixed lonPlus = lon + Fixed(360);
                            Fixed lonMinus = lon - Fixed(360);
                            inLon = ( lonPlus >= aLon && lonPlus <= bLon ) || ( lonMinus >= aLon && lonMinus <= bLon );
                        }

                        if( inLat && inLon )
                        {
                            if( removeMode )
                                g_app->GetWorldRenderer()->RemoveFromSelection( obj->m_objectId );
                            else
                                g_app->GetWorldRenderer()->AddToSelection( obj->m_objectId );
                        }
                    }
                    consumedByMarquee = true;
                }
                m_isDraggingSelect = false;
            }

            if( !consumedByMarquee )
            {
                if( m_currentStateId != -1 )                            
                {             
                    HandleClickStateMenu();
                }
                else if( underMouse )
                {
                    // If we have selection that can attack the object under mouse, give attack order (enables targeting cities, silos, etc.)
                    int selCount = g_app->GetWorldRenderer()->GetSelectionCount();
                    bool hasValidCombatTarget = false;
                    for( int si = 0; si < selCount; ++si )
                    {
                        int recId = g_app->GetWorldRenderer()->GetSelectedId( si );
                        WorldObject *obj = g_app->GetWorld()->GetWorldObject( recId );
                        if( obj && obj->IsValidCombatTarget( underMouseId ) > WorldObject::TargetTypeInvalid )
                        {
                            hasValidCombatTarget = true;
                            break;
                        }
                    }
                    if( hasValidCombatTarget )
                    {
                        if( (SelectionHasNukingUnit() || SelectionHasStandbyNukeUnit()) && underMouse->IsMovingObject() )
                        {
                            int resolvedId = ResolveNukeTargetForStatic( Fixed::FromDouble(longitude), Fixed::FromDouble(latitude) );
                            HandleObjectAction( longitude, latitude, resolvedId );
                        }
                        else
                        {
                            HandleObjectAction(longitude, latitude, underMouseId);
                        }
                    }
                    else if( SelectionHasStandbyNukeUnit() && underMouse->IsMovingObject() )
                    {
                        int resolvedId = ResolveNukeTargetForStatic( Fixed::FromDouble(longitude), Fixed::FromDouble(latitude) );
                        HandleObjectAction( longitude, latitude, resolvedId );
                    }
                    else if( underMouseId == g_app->GetWorldRenderer()->GetCurrentHighlightId() )
                    {
                        HandleSelectObject(underMouseId);
                    }
                }
                else if( !underMouse )
                {
                    if( g_app->GetWorldRenderer()->GetSelectionCount() > 0 )
                    {
                        if( SelectionHasNukingUnit() || SelectionHasLaunchableUnit() || SelectionHasStandbyNukeUnit() )
                            HandleObjectAction(longitude, latitude, -1);
                        else
                            HandleSetWaypoint( longitude, latitude, false );
                    }
                    else
                    {
                        g_app->GetWorldRenderer()->ClearSelection();
                    }
                }
            }
        }
#endif

        if( g_inputManager->m_rmbUnClicked )                                        // RIGHT MOUSE UNCLICKED
        {
            bool openedStateMenu = false;

            // Check for double-right-click (0.35s, 5.0 deg thresholds)
            bool isDoubleClick = false;
            float now = GetHighResTime();
            float timeSinceLastClick = now - m_lastRightClickTime;
            float distFromLastClick = sqrtf( (longitude - m_lastRightClickX) * (longitude - m_lastRightClickX) +
                                            (latitude - m_lastRightClickY) * (latitude - m_lastRightClickY) );
            if( timeSinceLastClick < 0.35f && distFromLastClick < 5.0f )
            {
                isDoubleClick = true;
            }
            m_lastRightClickTime = now;
            m_lastRightClickX = longitude;
            m_lastRightClickY = latitude;

            if( underMouseId != -1 )
            {
                WorldObject *obj = g_app->GetWorld()->GetWorldObject(underMouseId);
                if( obj && obj->m_teamId == g_app->GetWorld()->m_myTeamId )
                {
                    // Right-click on any friendly unit always opens the state menu
                    m_stateRenderTime = 10.0f;
                    m_stateObjectId = underMouseId;
                    m_stateApplyToSelectionSameType = g_app->GetWorldRenderer()->IsSelected( underMouseId );
                    g_soundSystem->TriggerEvent( "Interface", "HighlightObject" );
                    openedStateMenu = true;
                }
            }

            if( !openedStateMenu )
            {
                if( g_app->GetWorldRenderer()->GetSelectionCount() > 0 )
                {
                    // Silos and other static units take no waypoints - right-click is no-op
                    if( SelectionHasMovingUnit() )
                    {
                        if( isDoubleClick && underMouseId == -1 )
                        {
                            HandleSetWaypoint( longitude, latitude, true );
                        }
                        else
                        {
                            WorldObject *primaryObj = g_app->GetWorld()->GetWorldObject( g_app->GetWorldRenderer()->GetCurrentSelectionId() );
                            if( primaryObj &&
                                primaryObj->m_fleetId != -1 &&
                                underMouse &&
                                underMouse->m_fleetId == primaryObj->m_fleetId )
                            {
                                g_app->GetWorldRenderer()->SetCurrentSelectionId(-1);
                            }
                            else
                            {
                                HandleSetWaypoint( longitude, latitude, false );
                            }
                        }
                    }
                }

                if( underMouseId == -1 )
                {
                    m_stateRenderTime = 0.0f;
                    m_stateObjectId = -1;
                    m_stateApplyToSelectionSameType = false;
                    g_app->GetWorldRenderer()->SetCurrentHighlightId(-1);
                }
            }
        }
    }


    int oldHighlightId = g_app->GetWorldRenderer()->GetCurrentHighlightId();
    g_app->GetWorldRenderer()->SetCurrentHighlightId(-1);
    if( g_inputManager->m_lmbClicked )
    {
        if( underMouse &&
            underMouse->m_teamId == g_app->GetWorld()->m_myTeamId )                 // Mouse down on one of our own
        {
            g_app->GetWorldRenderer()->SetCurrentHighlightId(underMouseId);
            m_highlightTime = 0.0f;
        }
    }
    else if( g_inputManager->m_lmbUnClicked )                                // Mouse up
    {
        g_app->GetWorldRenderer()->SetCurrentHighlightId(-1);
        // Do NOT clear state menu when right-click also fired this frame - right-click opens the menu
        if( !g_inputManager->m_rmbUnClicked )
        {
            m_stateRenderTime = 0.0f;
            m_stateObjectId = -1;
            m_stateApplyToSelectionSameType = false;
        }
    }
    else if( !g_inputManager->m_lmb )                                 // Mouse highlights something of ours
    {
        if( !underMouse ||
            underMouse->IsCityClass() ||
            underMouseId == -1 ||
            g_app->GetWorld()->m_myTeamId == -1 ||
            g_app->GetGame()->m_winner ||
            underMouse->m_teamId == g_app->GetWorld()->m_myTeamId ||
            underMouse->m_visible[g_app->GetWorld()->m_myTeamId] ||
            underMouse->m_lastSeenTime[g_app->GetWorld()->m_myTeamId] > 0 )
        {
            g_app->GetWorldRenderer()->SetCurrentHighlightId(underMouseId);
            if( oldHighlightId != underMouseId )
            {
                m_highlightTime = 1.0f;
            }
        }
    }
    else if( g_inputManager->m_lmb && underMouseId != -1 )
    {
        g_app->GetWorldRenderer()->SetCurrentHighlightId(oldHighlightId);
    }
             
    int oldStateId = m_currentStateId;
    m_currentStateId = -1;
    if( m_stateRenderTime > 0.0f )  
    {
        m_stateRenderTime -= g_advanceTime;
        m_highlightTime = 0.0f;
        g_app->GetWorldRenderer()->SetCurrentHighlightId(m_stateObjectId);     
    }
    if( m_stateRenderTime <= 0.0f && m_stateObjectId != -1)
    {
        m_stateRenderTime = 0.0f;
        m_stateObjectId = -1;
        m_stateApplyToSelectionSameType = false;
    }

    if( m_stateRenderTime > 0.0f &&                                              // Player is choosing state changes
        g_app->GetWorldRenderer()->GetCurrentHighlightId() != -1 )
    {
    if( longitude < -180.0f ) longitude += 360.0f;
    if( longitude >180.0f ) longitude -= 360.0f;

        WorldObject *wobj = g_app->GetWorld()->GetWorldObject( g_app->GetWorldRenderer()->GetCurrentHighlightId() );
        if( wobj )
        {
            for( int i = 0; i < wobj->m_states.Size(); ++i )
            {
                float screenX, screenY, screenW, screenH;
                GetWorldObjectStatePosition( wobj, i, &screenX, &screenY, &screenW, &screenH );
                //if( screenX < -180 ) screenX += 360.0f;
                //if( screenX > 180 ) screenX -= 360.0f;
                if( longitude >= screenX && longitude <= screenX + screenW &&
                    latitude >= screenY && latitude <= screenY + screenH )
                {
                    m_currentStateId = i;
                    break;
                }
            }
            if( m_currentStateId == -1 )
            {
                if( wobj->IsActionQueueable() &&
                    wobj->m_actionQueue.Size() > 0 )
                {
                    float screenX, screenY, screenW, screenH;
                    GetWorldObjectStatePosition( wobj, -1, &screenX, &screenY, &screenW, &screenH );
                    if( longitude >= screenX && longitude <= screenX + screenW &&
                        latitude >= screenY && latitude <= screenY + screenH )
                    {
                        m_currentStateId = CLEARQUEUE_STATEID;
                    }
                }
            }
        }
    }

    if( g_keys[KEY_SPACE] )
    {
        g_app->GetWorldRenderer()->SetCurrentSelectionId(-1);
        UpdateSelectionAnimation(-1);
        m_stateRenderTime = 0.0f;
    }

    if( m_currentStateId != -1 &&
        m_currentStateId != oldStateId )
    {
        g_soundSystem->TriggerEvent( "Interface", "HighlightObjectState" );
    }

    m_lockCommands = false;
}

void MapRenderer::ConvertPixelsToAngle( float pixelX, float pixelY, float *longitude, float *latitude, bool absoluteLongitude)
{
    float width = 360.0f * m_zoomFactor;
    float aspect = (float) g_windowManager->WindowH() / (float) g_windowManager->WindowW();
    float height = (360.0f * aspect) * m_zoomFactor;

    float left, right, top, bottom;
    GetWindowBounds( &left, &right, &top, &bottom );

    *longitude = width * (pixelX - m_pixelX) / m_pixelW - width/2 + m_middleX;
    *latitude = height * ((m_pixelY+m_pixelH)-pixelY) / m_pixelH - height/2 + m_middleY;

    // correct for zoom factor
    *longitude -= (1.0f - m_zoomFactor ) / 3.0f;
    *latitude  -= (1.0f - m_zoomFactor ) / 3.0f;

    if( absoluteLongitude == false )
    {
        if( *longitude < -180.0f ) *longitude += 360.0f;
        if( *longitude >180.0f ) *longitude -= 360.0f;
    }
}


void MapRenderer::ConvertAngleToPixels( float longitude, float latitude, float *pixelX, float *pixelY )
{
    float left, right, top, bottom;
    GetWindowBounds( &left, &right, &top, &bottom );
    
    if( right > 180 )
    {
        right -= 360;
        left -= 360;
    }
    

    if( left < -180 && longitude > 0 )
    {
        left += 360;
        right += 360;
    }

    float fractionalLongitude = (longitude-left) / (right-left);
    float fractionalLatitude = (latitude-top) / (bottom-top);
    
    *pixelX = fractionalLongitude * (float)g_windowManager->WindowW();
    *pixelY = fractionalLatitude * (float)g_windowManager->WindowH();
}


void MapRenderer::GetWindowBounds( float *left, float *right, float *top, float *bottom )
{
    float width = 360.0f * m_zoomFactor;    
    float aspect = (float) g_windowManager->WindowH() / (float) g_windowManager->WindowW();
    float height = (360.0f * aspect) * m_zoomFactor;

    *left = m_middleX - width/2.0f;
    *top = m_middleY + height/2.0f;
    *right = *left+width;
    *bottom = *top-height;
}

float MapRenderer::GetZoomFactor()
{
    return m_zoomFactor;
}

float MapRenderer::GetDrawScale()
{
    return m_drawScale;
}


void MapRenderer::GetPosition( float &_middleX, float &_middleY )
{
    _middleX = m_middleX;
    _middleY = m_middleY;
}


int MapRenderer::GetLongitudeMod()
{
    if( m_middleX < 0 )
    {
        return 360;
    }
    else
    {
        return -360;
    }
}

void MapRenderer::UpdateSelectionAnimation( int id )
{
    m_selectTime = 1.0f;
    if( id == -1 )
    {
        m_deselectTime = 0.0f; 
    }
}

void MapRenderer::CenterViewport( float longitude, float latitude, int zoom, int camSpeed )
{
    m_cameraLongitude = longitude;
    m_cameraLatitude = latitude;

    float longDist = g_app->GetWorld()->GetDistance( Fixed::FromDouble(m_middleX), Fixed(0), Fixed::FromDouble(m_cameraLongitude), Fixed(0) ).DoubleValue();
    float latDist = g_app->GetWorld()->GetDistance( Fixed(0), Fixed::FromDouble(m_middleY), Fixed(0), Fixed::FromDouble(m_cameraLatitude) ).DoubleValue();

    m_speedRatioX = (longDist/latDist);
    if( m_speedRatioX < 0 )
    {
        m_speedRatioX = 1.0f;
    }
    m_speedRatioY = 1.0f;
  
    m_cameraZoom = zoom;
    m_camSpeed = camSpeed;

    m_camAtTarget = false;
}

void MapRenderer::CenterViewport( int objectId, int zoom, int camSpeed )
{
    WorldObject *obj = g_app->GetWorld()->GetWorldObject( objectId );
    if( obj )
    {
        CenterViewport( obj->m_longitude.DoubleValue(), obj->m_latitude.DoubleValue(), zoom, camSpeed );
    }
}

void MapRenderer::CameraCut( float longitude, float latitude, int zoom )
{
    m_middleX = longitude;
    m_middleY = latitude;
    m_totalZoom = zoom;
    m_cameraZoom = zoom;
}

void MapRenderer::FindScreenEdge( Vector3<float> const &_line, float *_posX, float *_posY )
{
//    y = mx + c
//    c = y - mx
//    x = (y - c) / m
 

    float left, right, top, bottom;
    g_app->GetMapRenderer()->GetWindowBounds( &left, &right, &top, &bottom );


    // shrink screen slightly so arrow renders fully onscreen

    top -= 2.5f * m_drawScale;
    right -= 2.5f * m_drawScale;
    bottom += 2.5f * m_drawScale;
    left += 2.5f * m_drawScale;

    float m = (_line.y - m_middleY) / (_line.x - m_middleX);
    float c = m_middleY - m * m_middleX;
    
    if( _line.y > top )
    {
        // Intersect with top view plane
        float x = ( top - c ) / m;
        if( x >= left && x <= right )
        {
            *_posX = x;
            *_posY = top;
            return;
        }
    }
    else
    {
        // Intersect with the bottom view plane
        float x = ( bottom - c ) / m;
        if( x >= left && x <= right )
        {
            *_posX = x;
            *_posY = bottom;
            return;
        }        
    }
 
    if( _line.x < left )
    {
        // Intersect with left view plane 
        float y = m * left + c;
        if( y >= bottom && y <= top ) 
        {
            *_posX = left;
            *_posY = y;
            return;
        }
    }
    else
    {
        // Intersect with right view plane
        float y = m * right + c;
        if( y >= bottom && y <= top ) 
        {
            *_posX = right;
            *_posY = y;
            return;
        }        
    }
 
    // We should never ever get here
    AppAbort( "We should never ever get here" );
}

void MapRenderer::UpdateMouseIdleTime()
{
    if( m_draggingCamera ) return;

    if( m_oldMouseX == g_inputManager->m_mouseX &&
        m_oldMouseY == g_inputManager->m_mouseY )
    {
        m_mouseIdleTime += SERVER_ADVANCE_PERIOD.DoubleValue();
    }
    else
    {
        m_oldMouseX = g_inputManager->m_mouseX;
        m_oldMouseY = g_inputManager->m_mouseY;
        m_mouseIdleTime = 0.0f;
    }
}

void MapRenderer::AutoCam()
{
    // automatically moves the camera to areas of interest every 10 seconds, or zooms out if nothing is happening
    m_autoCamTimer -= SERVER_ADVANCE_PERIOD.DoubleValue();
    if( m_autoCam && 
        (m_camAtTarget || m_autoCamTimer <= 0.0f) )
    {
        if( m_autoCamTimer <= 0.0f )
        {
            m_autoCamTimer = 20.0f;
            LList<Vector3<Fixed > *> areasOfInterest; // store longitude/latitude in x/y, and scene 'score' in z
        
            if( g_app->GetWorld()->m_gunfire.Size() > 0 )
            {
                for( int i = 0; i < g_app->GetWorld()->m_gunfire.Size(); ++i )
                {
                    if( g_app->GetWorld()->m_gunfire.ValidIndex(i) )
                    {
                        GunFire *gunfire = g_app->GetWorld()->m_gunfire[i];

                        if( gunfire->m_range > 5 ) // gf isnt about to expire
                        {
                            areasOfInterest.PutData( new Vector3<Fixed>(gunfire->m_longitude, gunfire->m_latitude, 1 ) );
                        }                    
                    }
                }
            }

            if( g_app->GetWorld()->m_explosions.Size() > 0 )
            {
                for( int i = 0; i < g_app->GetWorld()->m_explosions.Size(); ++i )
                {
                    if( g_app->GetWorld()->m_explosions.ValidIndex(i) )
                    {
                        Explosion *explosion = g_app->GetWorld()->m_explosions[i];

                        if( explosion->m_intensity >= 25 &&
                            explosion->m_intensity < 30 )
                        {
                            areasOfInterest.PutData( new Vector3<Fixed>( explosion->m_longitude, explosion->m_latitude, 1 ) );
                        }

                        if( explosion->m_intensity > 90 )
                        {
                            areasOfInterest.PutData( new Vector3<Fixed>( explosion->m_longitude, explosion->m_latitude, 10 ) );
                        }
                    }
                }
            }

            for( int i = 0; i < g_app->GetWorld()->m_messages.Size(); ++i )
            {
                if( g_app->GetWorld()->m_messages[i]->m_messageType == WorldMessage::TypeLaunch ||
                    g_app->GetWorld()->m_messages[i]->m_messageType == WorldMessage::TypeDirectHit )
                {
                    areasOfInterest.PutData( new Vector3<Fixed>( g_app->GetWorld()->m_messages[i]->m_longitude, g_app->GetWorld()->m_messages[i]->m_latitude, 20 ) );
                }

            }

            for( int i = 0; i < g_app->GetWorldRenderer()->GetAnimations().Size(); ++i )
            {
                if( g_app->GetWorldRenderer()->GetAnimations().ValidIndex(i) )
                {
                    if( g_app->GetWorldRenderer()->GetAnimations()[i]->m_animationType == g_app->GetWorldRenderer()->AnimationTypeNukePointer )
                    {
                        areasOfInterest.PutData( new Vector3<Fixed>( Fixed::FromDouble(g_app->GetWorldRenderer()->GetAnimations()[i]->m_longitude),
						 Fixed::FromDouble(g_app->GetWorldRenderer()->GetAnimations()[i]->m_latitude),
						 Fixed::FromDouble(15 + 5 * sfrand()) ) );
                    }
                }
            }

            for( int i = 0; i < areasOfInterest.Size(); ++i )
            {
                for( int j = 0; j < areasOfInterest.Size(); ++j )
                {
                    if( i != j )
                    {
                        if( g_app->GetWorld()->GetDistance( areasOfInterest[i]->x, areasOfInterest[i]->y, areasOfInterest[j]->x, areasOfInterest[j]->y ) <= 30 )
                        {
                            Vector3<Fixed> *vec = areasOfInterest[j];
                            delete vec;
                            areasOfInterest[i]->z += 1;
                            areasOfInterest.RemoveData(j);
                            j--;
                        }
                    }
                }
            }

            for( int i = 0; i < areasOfInterest.Size(); ++i )
            {
                if( areasOfInterest[i]->z < 2 )
                {
                    Vector3<Fixed> *vec = areasOfInterest[i];
                    delete vec;
                    areasOfInterest.RemoveData(i);
                    i--;
                }
            }


            if( areasOfInterest.Size() == 0 )
            {
                m_camAtTarget = true;
                CameraCut(0.0f, 0.0f, 0 );
                return;
            }

            m_autoCamCounter = 0;

            BoundedArray<int> topThree;
            BoundedArray<int> score;
            topThree.Initialise(3);
            score.Initialise(3);

            for( int i = 0; i < 3; ++i )
            {
                topThree[i] = -1;
                score[i] = 0;
            }
            int totalScore = 0;
            for( int i = 0; i < areasOfInterest.Size(); ++i )
            {
                totalScore += areasOfInterest[i]->z.IntValue();
                if( areasOfInterest[i]->z > score[0] )
                {
                    topThree[2] = topThree[1];
                    score[2] = score[1];

                    topThree[1] = topThree[0];
                    score[1] = score[0];

                    topThree[0] = i;
                    score[0] = areasOfInterest[i]->z.IntValue();
                }
                else if( areasOfInterest[i]->z > score[1] )
                {
                    topThree[2] = topThree[1];
                    score[2] = score[1];

                    topThree[1] = i;
                    score[1] = areasOfInterest[i]->z.IntValue();
                }
                else if( areasOfInterest[i]->z > score[2] )
                {
                    topThree[2] = i;
                    score[2] = areasOfInterest[i]->z.IntValue();
                }
            }

            int r = 0;
            if( areasOfInterest.Size() >= 9 )
            {
                int r = rand() % 3;
            }

            if( g_app->GetWorld()->GetDistance( Fixed::FromDouble(m_middleX), Fixed::FromDouble(m_middleY), areasOfInterest[ topThree[r] ]->x, areasOfInterest[ topThree[r] ]->y ) > 15 )
            {
                int zoom = ( (areasOfInterest[ topThree[r] ]->z / totalScore) * 20 ).IntValue();
                Clamp( zoom, 0, 20 );

                int camSpeed = g_app->GetWorld()->GetDistance( Fixed::FromDouble(m_middleX), Fixed::FromDouble(m_middleY), areasOfInterest[ topThree[r] ]->x,
															   areasOfInterest[ topThree[r] ]->y ).DoubleValue() / 180 * 70;
                camSpeed = max(20, camSpeed );

                if( camSpeed > 50 )
                {
                    CameraCut( areasOfInterest[ topThree[r] ]->x.DoubleValue(),
							   areasOfInterest[ topThree[r] ]->y.DoubleValue(), zoom );
                }
                else
                {
                    CenterViewport( areasOfInterest[ topThree[r] ]->x.DoubleValue(),
									areasOfInterest[ topThree[r] ]->y.DoubleValue(), zoom, camSpeed );
                }
            }
        }
    }
}

void MapRenderer::ToggleAutoCam()
{
    m_autoCam = !m_autoCam;
    if( m_autoCam )
    {
        m_lockCamControl = true;
    }
    else
    {
        m_lockCamControl = false;
    }
    m_cameraLongitude = 0.0f;
    m_cameraLatitude = 0.0f;
    m_cameraZoom = -1;
}


bool MapRenderer::GetAutoCam()
{
    return m_autoCam;
}

void MapRenderer::MoveCam()
{
    if(( m_cameraLongitude || m_cameraLatitude ) &&
        m_cameraIdleTime >= 1.0f )
    {
        m_lockCamControl = true;

        //m_totalZoom = m_cameraZoom;
        float scrollSpeed = m_camSpeed * m_zoomFactor * g_advanceTime;
        if( m_cameraLongitude )
        {
            if( m_middleX > m_cameraLongitude )
            {
                if( m_middleX - (scrollSpeed  * m_speedRatioX) < m_cameraLongitude )
                {
                    m_middleX = m_cameraLongitude;
                    m_cameraLongitude = 0.0f;
                    m_speedRatioX = 0.0f;
                }
                else
                {
                    m_middleX -= scrollSpeed * m_speedRatioX;
                }
            }
            else
            {
                if( m_middleX + (scrollSpeed  * m_speedRatioX)> m_cameraLongitude )
                {
                    m_middleX = m_cameraLongitude;
                    m_cameraLongitude = 0.0f;
                    m_speedRatioX = 0.0f;
                }
                else
                {
                    m_middleX += scrollSpeed * m_speedRatioX;
                }
            }
        }

        if( m_cameraLatitude )
        {
            if( m_middleY > m_cameraLatitude )
            {
                if( m_middleY - (scrollSpeed * m_speedRatioY )< m_cameraLatitude )
                {
                    m_middleY = m_cameraLatitude;
                    m_cameraLatitude = 0.0f;
                    m_speedRatioY = 0.0f;
                }
                else
                {
                    m_middleY -= scrollSpeed * m_speedRatioY;
                }
            }
            else
            {
                if( m_middleY + (scrollSpeed * m_speedRatioY) > m_cameraLatitude )
                {
                    m_middleY = m_cameraLatitude;
                    m_cameraLatitude = 0.0f;
                    m_speedRatioY = 0.0f;
                }
                else
                {
                    m_middleY += scrollSpeed * m_speedRatioY;
                }
            }
        }

        if( !m_cameraLongitude && !m_cameraLongitude )
        {
            m_camAtTarget = true;
            m_lockCamControl = false;
        }
        
    }

    if( m_camAtTarget && m_cameraZoom != -1 )
    {
        if( m_totalZoom != m_cameraZoom )
        {
            m_totalZoom = m_cameraZoom;
        }
        else
        {
            m_cameraZoom = -1;
        }
    }
}


bool MapRenderer::IsMouseInMapRenderer()
{    
    EclWindow *window = EclGetWindow();

    if( !window ) return true;

    if( stricmp(window->m_name, "comms window") == 0 &&
        stricmp(EclGetCurrentClickedButton(), "none") == 0 )
    {
        return true;
    }

    return false;
}


void MapRenderer::DragCamera()
{
    if( g_inputManager->m_mmb )
    {
        float left, right, top, bottom;
        GetWindowBounds( &left, &right, &top, &bottom );

        g_app->SetMousePointerVisible ( false );

        m_middleX -= g_inputManager->m_mouseVelX * m_drawScale * 0.2f;
        m_middleY += g_inputManager->m_mouseVelY * m_drawScale * 0.2f;
        m_draggingCamera = true;
        m_lockCommands = true;
    }
    else if( g_inputManager->m_mmbUnClicked && m_draggingCamera )
    {
        m_lockCommands = true;
    }
    else
    {
        if( !g_app->MousePointerIsVisible() )
        {
            g_app->SetMousePointerVisible ( true );
        }
        m_draggingCamera = false;
    }
}

bool MapRenderer::IsOnScreen( float _longitude, float _latitude, float _expandScreen )
{
    float left, right, top, bottom;
    GetWindowBounds( &left, &right, &top, &bottom );

    if( _expandScreen > 0.0f )
    {
        left -= 2.0f;
        right += 2.0f;
        top += 2.0f;
        bottom -= 2.0f;
    }

    if( _longitude > left && _longitude < right &&
        _latitude > bottom && _latitude < top )
    {
        return true;
    }

    if( left < -180.0f )
    {
        if(((_longitude > left+360.0f && _longitude < 180.0f) ||
            (_longitude < right )) &&
            _latitude > bottom && _latitude < top )
        {
            return true;
        }
    }

    if( right > 180.0f )
    {
        if(((_longitude < right-360.0f && _longitude > -180.0f) ||
            (_longitude > left )) &&
            _latitude > bottom && _latitude < top )
        {
            return true;
        }
    }
    return false;
}

static float GetDistancePoint( float longitude1, float latitude1, float longitude2, float latitude2 )
{
	float distdx = 0.0f;
	float distdy = 0.0f;

	if ( -90.0f >= longitude1 && longitude2 >= 90.0f )
	{
		float dx1 = -180.0f - longitude1;
		float dx2 = longitude2 - 180.0f;
		float dx = dx1 + dx2;
		float dy = latitude2 - latitude1;
		float midLatitude = latitude1;
		if ( dx != 0.0f )
			midLatitude += dy * ( dx1 / dx );
		distdx = -180.0f - longitude1;
		distdy = midLatitude - latitude1;
		distdx += longitude2 - 180.0f;
		distdy += latitude2 - midLatitude;
	}
	else if ( 90.0f <= longitude1 && longitude2 <= -90.0f )
	{
		float dx1 = 180.0f - longitude1;
		float dx2 = longitude2 - -180.0f;
		float dx = dx1 + dx2;
		float dy = latitude2 - latitude1;
		float midLatitude = latitude1;
		if ( dx != 0.0f )
			midLatitude += dy * ( dx1 / dx );
		distdx = 180.0f - longitude1;
		distdy = midLatitude - latitude1;
		distdx += longitude2 - -180.0f;
		distdy += latitude2 - midLatitude;
	}
	else
	{
		distdx = longitude2 - longitude1;
		distdy = latitude2 - latitude1;
	}

	return sqrt( distdx * distdx + distdy * distdy );
}

bool MapRenderer::UpdatePlanning( float longitude, float latitude )
{
	if( !g_app->GetWorldRenderer()->GetEditWhiteBoard() || !g_app->GetWorldRenderer()->GetShowPlanning() )
	{
		g_app->GetWorldRenderer()->SetDrawingPlanning( false );
		g_app->GetWorldRenderer()->SetErasingPlanning( false );
		return false;
	}

    if( g_app->GetWorldRenderer()->GetShowPlanning() && g_keys[KEY_SPACE] )
    {
		g_app->GetWorldRenderer()->SetDrawingPlanning( false );
		g_app->GetWorldRenderer()->SetErasingPlanning( false );
		g_app->GetWorldRenderer()->SetShowPlanning( false );
		return false;
    }

	Team *myTeam = g_app->GetWorld()->GetMyTeam();
	if ( !myTeam )
	{
		g_app->GetWorldRenderer()->SetDrawingPlanning( false );
		g_app->GetWorldRenderer()->SetErasingPlanning( false );
		return false;
	}

	if ( !IsMouseInMapRenderer() )
	{
		g_app->GetWorldRenderer()->SetDrawingPlanning( false );
		g_app->GetWorldRenderer()->SetErasingPlanning( false );
		return true;
	}

	WhiteBoard *whiteBoard = &g_app->GetWorld()->m_whiteBoards[ myTeam->m_teamId ];

	if( longitude < -180.0f ) longitude += 360.0f;
    if( longitude > 180.0f ) longitude -= 360.0f;

	// Drawing part
	if( g_inputManager->m_lmbClicked || ( g_inputManager->m_lmb && !g_app->GetWorldRenderer()->GetDrawingPlanning() ) )
	{
		g_app->GetWorldRenderer()->SetErasingPlanning( false );
		g_app->GetWorldRenderer()->SetDrawingPlanning( true );
		g_app->GetWorldRenderer()->SetDrawingPlanningTime( 0.0f );
		whiteBoard->RequestStartLine( longitude, latitude );
		m_longitudePlanningOld = longitude;
		m_latitudePlanningOld = latitude;
	}
	else if( g_inputManager->m_lmb || ( g_inputManager->m_lmbUnClicked && g_app->GetWorldRenderer()->GetDrawingPlanning() ) )
	{
		g_app->GetWorldRenderer()->SetErasingPlanning( false );
		g_app->GetWorldRenderer()->SetDrawingPlanningTime( g_app->GetWorldRenderer()->GetDrawingPlanningTime() + g_advanceTime );
		if ( g_app->GetWorldRenderer()->GetDrawingPlanningTime() >= 0.03333f || g_inputManager->m_lmbUnClicked || 
		     ( g_app->GetWorldRenderer()->GetDrawingPlanningTime() >= 0.01667f && GetDistancePoint ( m_longitudePlanningOld, m_latitudePlanningOld, longitude, latitude ) >= WhiteBoard::LINE_LENGTH_MAX ) )
		{
			g_app->GetWorldRenderer()->SetDrawingPlanningTime( 0.0f );
			if ( m_longitudePlanningOld != longitude || m_latitudePlanningOld != latitude )
			{
				if ( -90.0f >= m_longitudePlanningOld && longitude >= 90.0f )
				{
					float dx1 = -180.0f - m_longitudePlanningOld;
					float dx2 = longitude - 180.0f;
					float dx = dx1 + dx2;
					float dy = latitude - m_latitudePlanningOld;
					float midLatitude = m_latitudePlanningOld;
					if ( dx != 0.0f )
						midLatitude += dy * ( dx1 / dx );
					whiteBoard->RequestAddLinePoint( m_longitudePlanningOld, m_latitudePlanningOld, -180.0f, midLatitude );
					whiteBoard->RequestStartLine( 180.0f, midLatitude );
					whiteBoard->RequestAddLinePoint ( 180.0f, midLatitude, longitude, latitude );
				}
				else if ( 90.0f <= m_longitudePlanningOld && longitude <= -90.0f )
				{
					float dx1 = 180.0f - m_longitudePlanningOld;
					float dx2 = longitude - -180.0f;
					float dx = dx1 + dx2;
					float dy = latitude - m_latitudePlanningOld;
					float midLatitude = m_latitudePlanningOld;
					if ( dx != 0.0f )
						midLatitude += dy * ( dx1 / dx );
					whiteBoard->RequestAddLinePoint( m_longitudePlanningOld, m_latitudePlanningOld, 180.0f, midLatitude );
					whiteBoard->RequestStartLine( -180.0f, midLatitude );
					whiteBoard->RequestAddLinePoint ( -180.0f, midLatitude, longitude, latitude );
				}
				else
				{
					whiteBoard->RequestAddLinePoint ( m_longitudePlanningOld, m_latitudePlanningOld, longitude, latitude );
				}

				m_longitudePlanningOld = longitude;
				m_latitudePlanningOld = latitude;
			}
		}

		if ( g_inputManager->m_lmbUnClicked )
		{
			g_app->GetWorldRenderer()->SetDrawingPlanning( false );
		}
	}
	// Erasing part
	else if( g_inputManager->m_rmbClicked || ( g_inputManager->m_rmb && !g_app->GetWorldRenderer()->GetErasingPlanning() ) )
	{
		g_app->GetWorldRenderer()->SetDrawingPlanning( false );
		g_app->GetWorldRenderer()->SetErasingPlanning( true );
		g_app->GetWorldRenderer()->SetDrawingPlanningTime( 0.0f );
		m_longitudePlanningOld = longitude;
		m_latitudePlanningOld = latitude;
	}
	else if( g_inputManager->m_rmb || ( g_inputManager->m_rmbUnClicked && g_app->GetWorldRenderer()->GetErasingPlanning() ) )
	{
		g_app->GetWorldRenderer()->SetDrawingPlanning( false );
		g_app->GetWorldRenderer()->SetDrawingPlanningTime( g_app->GetWorldRenderer()->GetDrawingPlanningTime() + g_advanceTime );
		if ( g_app->GetWorldRenderer()->GetDrawingPlanningTime() >= 0.03333f || g_inputManager->m_rmbUnClicked )
		{
			g_app->GetWorldRenderer()->SetDrawingPlanningTime( 0.0f );
			if ( -90.0f >= m_longitudePlanningOld && longitude >= 90.0f )
			{
				float dx1 = -180.0f - m_longitudePlanningOld;
				float dx2 = longitude - 180.0f;
				float dx = dx1 + dx2;
				float dy = latitude - m_latitudePlanningOld;
				float midLatitude = m_latitudePlanningOld;
				if ( dx != 0.0f )
					midLatitude += dy * ( dx1 / dx );
				whiteBoard->RequestErase( m_longitudePlanningOld, m_latitudePlanningOld, -180.0f, midLatitude );
				whiteBoard->RequestErase( 180.0f, midLatitude, longitude, latitude );
			}
			else if ( 90.0f <= m_longitudePlanningOld && longitude <= -90.0f )
			{
				float dx1 = 180.0f - m_longitudePlanningOld;
				float dx2 = longitude - -180.0f;
				float dx = dx1 + dx2;
				float dy = latitude - m_latitudePlanningOld;
				float midLatitude = m_latitudePlanningOld;
				if ( dx != 0.0f )
					midLatitude += dy * ( dx1 / dx );
				whiteBoard->RequestErase( m_longitudePlanningOld, m_latitudePlanningOld, 180.0f, midLatitude );
				whiteBoard->RequestErase( -180.0f, midLatitude, longitude, latitude );
			}
			else
			{
				whiteBoard->RequestErase ( m_longitudePlanningOld, m_latitudePlanningOld, longitude, latitude );
			}

			m_longitudePlanningOld = longitude;
			m_latitudePlanningOld = latitude;
		}

		if ( g_inputManager->m_rmbUnClicked )
		{
			g_app->GetWorldRenderer()->SetErasingPlanning( false );
		}
	}
	else
	{
		g_app->GetWorldRenderer()->SetDrawingPlanning( false );
		g_app->GetWorldRenderer()->SetErasingPlanning( false );
	}

	return true;
}

void MapRenderer::RenderWhiteBoard()
{
	if ( !g_app->GetWorldRenderer()->GetShowWhiteBoard() && !g_app->GetWorldRenderer()->GetEditWhiteBoard() ) 
	{
		return;
	}

	// Get effective team for whiteboard viewing - use perspective team if set, otherwise myTeam
	Team *effectiveTeam = g_app->GetWorldRenderer()->GetEffectiveWhiteBoardTeam();
	if( !effectiveTeam )
	{
		return;
	}

	START_PROFILE( "WhiteBoard" );

	int sizeteams = g_app->GetWorld()->m_teams.Size();
    for( int i = 0; i < sizeteams; ++i )
    {
        Team *team = g_app->GetWorld()->m_teams[ i ];

		if ( ( g_app->GetWorldRenderer()->GetShowAllWhiteBoards() && g_app->GetWorld()->IsFriend ( effectiveTeam->m_teamId, team->m_teamId ) ) || 
		     effectiveTeam->m_teamId == team->m_teamId )
		{
			WhiteBoard *whiteBoard = &g_app->GetWorld()->m_whiteBoards[ team->m_teamId ];
			char whiteboardname[32];
			snprintf( whiteboardname, sizeof(whiteboardname), "WhiteBoard%d", team->m_teamId );

			Colour colourBoard = team->GetTeamColour();

			const LList<WhiteBoardPoint *> *points = whiteBoard->GetListPoints();
			int sizePoints = points->Size();

			if ( sizePoints > 1 )
			{
				WhiteBoardPoint *prevPt = points->GetData( 0 );
				
				for ( int j = 1; j < sizePoints; j++ )
				{
					WhiteBoardPoint *currPt = points->GetData( j );
                    float lineWidth = 2.0f;

                    //
					// if this is a start point, skip the line from previous point

					if ( currPt->m_startPoint )
					{
						prevPt = currPt;
						continue;
					}

					g_renderer2d->Line( prevPt->m_longitude, prevPt->m_latitude, 
									  currPt->m_longitude, currPt->m_latitude, 
									  colourBoard, lineWidth );
					
					prevPt = currPt;
				}
			}
		}
	}

	END_PROFILE( "WhiteBoard" );
}

void MapRenderer::RenderSanta()
{
#ifdef ENABLE_SANTA_EASTEREGG
	if ( g_app->GetWorld()->m_santaAlive )
	{

        
		Fixed predictionTime = Fixed::FromDouble(g_predictionTime) * g_app->GetWorld()->GetTimeScaleFactor();
		Fixed currentTime = g_app->GetWorld()->m_theDate.m_theDate + predictionTime;
		if ( currentTime > g_app->GetWorld()->m_santaRouteTotal )
		{
			while ( currentTime > g_app->GetWorld()->m_santaRouteTotal )
			{
				currentTime -= g_app->GetWorld()->m_santaRouteTotal;
			}
			g_app->GetWorld()->m_santaCurrent = 0;
		}
        
        while ( g_app->GetWorld()->m_santaRouteLength.ValidIndex( g_app->GetWorld()->m_santaCurrent ) &&
                g_app->GetWorld()->m_santaRouteLength[ g_app->GetWorld()->m_santaCurrent ] < currentTime )
        {
            ++g_app->GetWorld()->m_santaCurrent;
		}

        float size = 1.5f;
		float x;
		float y;
		float thisSize = size*2;

        if( g_app->GetWorld()->m_santaCurrent >= g_app->GetWorld()->m_santaRouteLength.Size() ) 
        {
            g_app->GetWorld()->m_santaAlive = false;
            return;
        }
        
		//if index is odd then we are inside a city so don't render santa
		if ( g_app->GetWorld()->m_santaCurrent % 2 == 0 )
		{
			City *cityStart;
			City *cityEnd;

			int index = g_app->GetWorld()->m_santaCurrent / 2;
			cityStart = g_app->GetWorld()->m_santaRoute.GetData( index );
			if ( index == g_app->GetWorld()->m_santaRoute.Size() - 1 )
			{
				cityEnd =  g_app->GetWorld()->m_santaRoute.GetData( 0 );
			}
			else
			{
				cityEnd =  g_app->GetWorld()->m_santaRoute.GetData( index + 1 );
			}

			Fixed prevTime;
			if ( g_app->GetWorld()->m_santaCurrent == 0 )
			{
				prevTime = 0;
			}
			else
			{
				prevTime = g_app->GetWorld()->m_santaRouteLength.GetData( g_app->GetWorld()->m_santaCurrent - 1 );
			}
			Fixed endTime = g_app->GetWorld()->m_santaRouteLength.GetData( g_app->GetWorld()->m_santaCurrent );

			Fixed dist = currentTime - prevTime;
			Fixed totalDist = endTime - prevTime;
			
			x = BezierCurve( ( dist / totalDist ).DoubleValue(), cityEnd->m_longitude.DoubleValue(), cityEnd->m_longitude.DoubleValue(), 
									cityStart->m_longitude.DoubleValue(), cityStart->m_longitude.DoubleValue() );
			y = BezierCurve( ( dist / totalDist ).DoubleValue(), cityEnd->m_latitude.DoubleValue(), cityEnd->m_latitude.DoubleValue(), 
									cityStart->m_latitude.DoubleValue(), cityStart->m_latitude.DoubleValue() );

			g_app->GetWorld()->m_santaLongitude = Fixed::FromDouble( x );
			g_app->GetWorld()->m_santaLatitude = Fixed::FromDouble( y );

            if( cityStart->m_longitude <= cityEnd->m_longitude )
            {
				g_app->GetWorld()->m_santaPrevFlipped = false;
            }
            else
            {
				g_app->GetWorld()->m_santaPrevFlipped = true;
            }


			Colour colour       = White;
			colour.m_a = 255;

			Image *bmpImage = g_resource->GetImage( "graphics/santa.bmp" );
			if( bmpImage )
			{
				if ( g_app->GetWorld()->m_santaPrevFlipped )
				{
					g_renderer2d->RotatingSprite( bmpImage, x, y, -thisSize, thisSize, colour, 0 );
				}
				else
				{
					g_renderer2d->RotatingSprite( bmpImage, x, y, thisSize, thisSize, colour, 0 );
				}			
			}
		}
		else
		{
			City *city;

			int index = ( g_app->GetWorld()->m_santaCurrent + 1 )/ 2;
			if (index >= g_app->GetWorld()->m_santaRoute.Size())
			{
				index = 0;
			}
			city = g_app->GetWorld()->m_santaRoute.GetData( index );

			Fixed testx = city->m_longitude;
			Fixed testy = city->m_latitude;

			float x = testx.DoubleValue();
			float y = testy.DoubleValue();

			Colour colour       = White;
			colour.m_a = 255;

			Image *bmpImage = g_resource->GetImage( "graphics/santa.bmp" );
			if( bmpImage )
			{
				if ( g_app->GetWorld()->m_santaPrevFlipped )
				{
					g_renderer2d->RotatingSprite( bmpImage, x, y, -thisSize, thisSize, colour, 0 );
				}
				else
				{
					g_renderer2d->RotatingSprite( bmpImage, x, y, thisSize, thisSize, colour, 0 );
				}		
			}		
		}
		
	}

    //Nuke Santa and thereby end Christmas for everyone, for ever more
#endif
}