#include "lib/universal_include.h"
#include "lib/gucci/input.h"
#include "lib/render2d/renderer.h"
#include "lib/sound/sound_parameter.h"
#include "lib/sound/sound_instance.h"
#include "lib/sound/soundsystem.h"

#include "interface/components/drop_down_menu.h"
#include "interface/components/inputfield.h"

#include "soundparameter_window.h"



//*****************************************************************************
// Class SoundParameterButton
//*****************************************************************************

SoundParameterButton::SoundParameterButton()
:   TextButton(),
    m_parameter(NULL),
    m_minOutput(0.0f),
    m_maxOutput(1.0f)
{
}


void SoundParameterButton::Render( int realX, int realY, bool highlighted,  bool clicked )
{
    float editAreaWidth = m_w - 100;
    g_renderer->EclipseRectFill( realX + m_w - editAreaWidth, realY, editAreaWidth, m_h, Colour(25, 0, 0, 128) );

    // Top line
    g_renderer->EclipseLine( realX + m_w - editAreaWidth, realY, realX + m_w, realY, Colour(0, 0, 0, 255) );
    // Left line
    g_renderer->EclipseLine( realX + m_w - editAreaWidth, realY, realX + m_w - editAreaWidth, realY+m_h, Colour(0, 0, 0, 255) );
    // Right line
    g_renderer->EclipseLine( realX + m_w, realY, realX + m_w, realY + m_h, Colour(229, 76, 76, 76) );
    // Bottom line
    g_renderer->EclipseLine( realX + m_w - editAreaWidth, realY+m_h, realX + m_w, realY + m_h, Colour(100, 34, 34, 150) );


    TextButton::Render( realX, realY, highlighted, clicked );

    float value = m_parameter->GetOutput();
    
    switch( m_parameter->m_type )
    {
        case SoundParameter::TypeFixedValue:
        {
            g_renderer->TextRight( realX + m_w - 10, realY+4, White, 12, "Fixed at %2.2f", value );
            break;
        }

        case SoundParameter::TypeRangedRandom:
        {
            g_renderer->TextRight( realX + m_w - 5, realY+4, White, 12, "From %2.2f to %2.2f", 
                                            m_parameter->m_outputLower, m_parameter->m_outputUpper );
            break;
        }

        case SoundParameter::TypeLinked:
        {
            g_renderer->TextRight( realX + m_w - 10, realY+4, White, 12, "Linked To %s", 
                                            m_parameter->GetLinkName( m_parameter->m_link ) );
            break;
        }
        
        default:
            g_renderer->TextRightSimple( realX + m_w - 10, realY+4, White, 12, "[???]" );
    }
}


void SoundParameterButton::MouseUp()
{
    SoundParameterEditor *editor = new SoundParameterEditor( m_caption );
    editor->SetSize( 400.0f, 400.0f );
    editor->m_parameter = m_parameter;
    editor->m_minOutput = m_minOutput;
    editor->m_maxOutput = m_maxOutput;
    EclRegisterWindow( editor, m_parent );
}


//*****************************************************************************
// Class SoundParameterGraph
//*****************************************************************************

void SoundParameterGraph::SetParameter( SoundParameter *_parameter, float _minOutput, float _maxOutput )
{
    m_parameter = _parameter;
    m_minOutput = _minOutput;
    m_maxOutput = _maxOutput;
}


void SoundParameterGraph::GetPosition( float _output, float _input, float *_x, float *_y )
{
    float minX = 50;
    float maxX = m_w - 20;
    float minY = m_h - 40;
    float maxY = 30;
    float sizeX = maxX - minX;
    float sizeY = maxY - minY;

//    if( m_minOutput != 0 )
//    {
//        float sizeBelowZero = m_minOutput;
//        float fractionBelowZero = m_minOutput / (m_maxOutput - m_minOutput);
//        minX += sizeX * fractionBelowZero * -1.0f;
//    }
    
    float fractionOutput = (_output - m_minOutput) / (m_maxOutput - m_minOutput);
    float fractionInput = _input / ( m_maxInput - m_minInput );

	if (m_linearScale)
	{
		if( _x ) *_x = minX + fractionOutput * sizeX;
	}
	else
	{
		if( _x ) *_x = minX + sqrtf(fractionOutput) * sizeX;
	}
    if( _y ) *_y = minY + (fractionInput) * sizeY;
}


void SoundParameterGraph::GetValues( float _x, float _y, float *_output, float *_input )
{
    float minX = 50;
    float maxX = m_w - 20;
    float minY = m_h - 40;
    float maxY = 30;
    float sizeX = maxX - minX;
    float sizeY = maxY - minY;

    float fractionX = (_x - minX) / ( maxX - minX );
	if (!m_linearScale)
	{
		fractionX *= fractionX;
	}
    float fractionY = (_y - minY) / ( maxY - minY );

    if( _output ) *_output = m_minOutput + ( m_maxOutput - m_minOutput ) * fractionX;
    if( _input ) *_input = m_minInput + ( m_maxInput - m_minInput ) * fractionY;
}


void SoundParameterGraph::RescaleAxis()
{
    switch( m_parameter->m_type )
    {
        case SoundParameter::TypeFixedValue:
        case SoundParameter::TypeRangedRandom:
            m_minInput = 0.0f;
            m_maxInput = 1.0f;
            break;

        case SoundParameter::TypeLinked:
        {
            const char *paramName = SoundParameter::GetLinkName(m_parameter->m_link);
            g_soundSystem->m_interface->GetPropertyRange( paramName, &m_minInput, &m_maxInput );
            break;
        }
    }

    m_parameter->m_inputLower  = max( m_parameter->m_inputLower, m_minInput );
    m_parameter->m_inputLower  = min( m_parameter->m_inputLower, m_maxInput );
    
    m_parameter->m_inputUpper  = max( m_parameter->m_inputUpper, m_minInput );
    m_parameter->m_inputUpper  = min( m_parameter->m_inputUpper, m_maxInput );
    
    m_parameter->m_outputLower = max( m_parameter->m_outputLower, m_minOutput );
    m_parameter->m_outputLower = min( m_parameter->m_outputLower, m_maxOutput );

    m_parameter->m_outputUpper = max( m_parameter->m_outputUpper, m_minOutput );
    m_parameter->m_outputUpper = min( m_parameter->m_outputUpper, m_maxOutput );
}


void SoundParameterGraph::HandleMouseEvents()
{
    if( EclMouseInButton(m_parent,this) )                    
    {
		bool lmb = g_inputManager->m_lmb;
        bool rmb = g_inputManager->m_rmb;

        float input, output;
        GetValues( g_inputManager->m_mouseX - ( m_parent->m_x + m_x ),
                   g_inputManager->m_mouseY - ( m_parent->m_y + m_y ),
                   &output, &input );

        if( output < m_minOutput ) output = m_minOutput;
        if( output > m_maxOutput ) output = m_maxOutput;

        if( input < m_minInput ) input = m_minInput;
        if( input > m_maxInput ) input = m_maxInput;
        
        if( g_inputManager->m_mouseX < m_parent->m_x + m_x + 50 )
        {
            SoundInstance *instance = g_soundSystem->GetSoundInstance( g_soundSystem->m_editorInstanceId );
            if( lmb && instance &&
                m_parameter->m_type == SoundParameter::TypeLinked )
            {
                instance->ForceParameter( *m_parameter, input );
            }
        }
        else
        {
            switch( m_parameter->m_type )
            {
                case SoundParameter::TypeFixedValue:
                    if( lmb )
                    {
                        m_parameter->m_outputLower = output;
                        m_parameter->Recalculate();
                    }
                    break;

                case SoundParameter::TypeRangedRandom:
                    if( lmb )
                    {
                        m_parameter->m_outputLower = output;
                        m_parameter->m_outputUpper = max( m_parameter->m_outputUpper, 
                                                          m_parameter->m_outputLower );                        
                        m_parameter->Recalculate();
                    }
                    if( rmb )
                    {
                        m_parameter->m_outputUpper = output;
                        m_parameter->m_outputLower = min( m_parameter->m_outputLower,
                                                          m_parameter->m_outputUpper );
                        m_parameter->Recalculate();
                    }
                    break;
                    
                case SoundParameter::TypeLinked:
                    if( lmb )
                    {
                        m_parameter->m_inputLower = input;
                        m_parameter->m_outputLower = output;
                        m_parameter->m_outputUpper = max( m_parameter->m_outputUpper, 
                                                          m_parameter->m_outputLower );
                    }
                    if( rmb )
                    {
                        m_parameter->m_inputUpper = input;
                        m_parameter->m_outputUpper = output;
                        m_parameter->m_outputLower = min( m_parameter->m_outputLower, 
                                                          m_parameter->m_outputUpper );
                    }
                    break;
            }
        }
    }
}


void SoundParameterGraph::RenderAxis( int realX, int realY )
{
    float x1, x2, y;
    GetPosition( m_minOutput, 0, &x1, &y );
    GetPosition( m_maxOutput, 0, &x2, &y );
    
    float y1, y2, x;
    GetPosition( 0, m_minInput, &x, &y2 );
    GetPosition( 0, m_maxInput, &x, &y1 );

    // Convert axis rendering to modern renderer Line calls
    // Main horizontal axis line
    g_renderer->EclipseLine( realX + x1, realY + y, realX + x2, realY + y, White );
    
    // Vertical axis line (if linked parameter)
    if( m_parameter->m_type == SoundParameter::TypeLinked )
    {
        g_renderer->EclipseLine( realX + x, realY + y1, realX + x, realY + y2, White );
    }

    g_renderer->Text( realX + x1, realY + y + 20, White, 10, "%2.2f", m_minOutput );
    g_renderer->Text( realX + x2, realY + y + 20, White, 10, "%2.2f", m_maxOutput );

    if( m_parameter->m_type == SoundParameter::TypeLinked )
    {
        char caption[256];
        sprintf( caption, "%2.2f", m_maxInput );
        g_renderer->TextSimple( realX + x, realY + y1, White, 10, caption );

        sprintf( caption, "%2.2f", m_minInput );
        g_renderer->TextSimple( realX + x, realY + y2, White, 10, caption );
    }

	
	//
	// Render lines every 10% along the scale

	{
		// Convert scale lines to modern renderer Line calls
		float x1, y, y1, y2;
		GetPosition( 0, m_minInput, &x1, &y1 );
		GetPosition( 0, m_maxInput, &x1, &y2 );
		for (int i = 0; i < 11; ++i)
		{
			float output = m_minOutput + (float)i * 0.1f * (m_maxOutput - m_minOutput);
			GetPosition( output, 0, &x1, &y );
			g_renderer->EclipseLine( realX + x1, realY + y1, realX + x1, realY + y2, Colour(60, 60, 70, 255) );
		}
	}
}


void SoundParameterGraph::RenderValues( int realX, int realY )
{
    switch( m_parameter->m_type )
    {
        case SoundParameter::TypeFixedValue:
        {
            float x, y1, y2;
            GetPosition( m_parameter->m_outputLower, m_minInput, &x, &y1 );
            GetPosition( m_parameter->m_outputLower, m_maxInput, &x, &y2 );
            // Convert fixed value line to modern renderer
            g_renderer->EclipseLine( realX + x, realY + y1, realX + x, realY + y2, Colour(179, 204, 255, 179) );
            g_renderer->Text( realX + x, realY + y1 + 10, White, 10, "%2.2f", m_parameter->m_outputLower );
            break;
        }

        case SoundParameter::TypeRangedRandom:
        {
            float x1, x2, y1, y2;
            GetPosition( m_parameter->m_outputLower, m_minInput, &x1, &y1 );
            GetPosition( m_parameter->m_outputLower, m_maxInput, &x1, &y2 );
            GetPosition( m_parameter->m_outputUpper, m_minInput, &x2, NULL );

            // Convert ranged random visualization to modern renderer
            g_renderer->EclipseRectFill( realX + x1, realY + y1, x2 - x1, y2 - y1, Colour(128, 128, 255, 76) );
            
            // Border lines
            g_renderer->EclipseLine( realX + x1, realY + y1, realX + x1, realY + y2, Colour(128, 128, 255, 179) );
            g_renderer->EclipseLine( realX + x2, realY + y1, realX + x2, realY + y2, Colour(128, 128, 255, 179) );

            g_renderer->TextCentre( realX + x1, realY + y1 + 10, White, 10, "%2.2f", m_parameter->m_outputLower );
            g_renderer->TextCentre( realX + x2, realY + y1 + 10, White, 10, "%2.2f", m_parameter->m_outputUpper );

            break;
        }

        case SoundParameter::TypeLinked:
        {
            float x1, x2, y1, y2;
            float minX, minY, maxX, maxY;                
            GetPosition( m_parameter->m_outputLower, m_parameter->m_inputLower, &x1, &y1 );
            GetPosition( m_parameter->m_outputUpper, m_parameter->m_inputUpper, &x2, &y2 );
            GetPosition( m_minOutput, m_minInput, &minX, &minY );
            GetPosition( m_maxOutput, m_maxInput, &maxX, &maxY );
            
            // Convert linked parameter visualization to modern renderer
            // Main connection line
            g_renderer->EclipseLine( realX + x1, realY + y1, realX + x2, realY + y2, Colour(128, 128, 255, 230) );
            
            // Vertical indicator lines
            if( y1 >= y2 ) {
                g_renderer->EclipseLine( realX + x1, realY + y1, realX + x1, realY + minY, Colour(128, 128, 255, 230) );
                g_renderer->EclipseLine( realX + x2, realY + y2, realX + x2, realY + maxY, Colour(128, 128, 255, 230) );
            } else {
                g_renderer->EclipseLine( realX + x1, realY + y1, realX + x1, realY + maxY, Colour(128, 128, 255, 230) );
                g_renderer->EclipseLine( realX + x2, realY + y2, realX + x2, realY + minY, Colour(128, 128, 255, 230) );
            }

            g_renderer->TextCentre( realX + x1, realY + minY + 10, White, 10, "%2.2f", m_parameter->m_outputLower );
            g_renderer->TextCentre( realX + x2, realY + minY + 10, White, 10, "%2.2f", m_parameter->m_outputUpper );

            g_renderer->TextCentre( realX + minX, realY + y1, White, 10, "%2.2f", m_parameter->m_inputLower );
            g_renderer->TextCentre( realX + minX, realY + y2, White, 10, "%2.2f", m_parameter->m_inputUpper );
            break;
        }
    }
}


void SoundParameterGraph::RenderOutput( int realX, int realY )
{
    if( m_parameter->m_type == SoundParameter::TypeRangedRandom )
    {
        SoundInstance *instance = g_soundSystem->GetSoundInstance( g_soundSystem->m_editorInstanceId );
        if( instance )
        {
            instance->UpdateParameter( *m_parameter );
        }
         
        float output = m_parameter->m_output;
        float x, minY, maxY;
        GetPosition( output, m_minInput, &x, &minY );
        GetPosition( output, m_maxInput, &x, &maxY );
        // Convert output visualization to modern renderer
        g_renderer->EclipseLine( realX + x, realY + minY, realX + x, realY + maxY, Colour(128, 255, 128, 191) );

        float desiredOutput = m_parameter->m_desiredOutput;
        GetPosition( desiredOutput, m_minInput, &x, &minY );
        GetPosition( desiredOutput, m_maxInput, &x, &maxY );
        g_renderer->EclipseLine( realX + x, realY + minY, realX + x, realY + maxY, Colour(128, 255, 128, 89) );
    }
    else if( m_parameter->m_type == SoundParameter::TypeLinked )
    {
        SoundInstance *instance = g_soundSystem->GetSoundInstance( g_soundSystem->m_editorInstanceId );
        if( instance )
        {
            instance->UpdateParameter( *m_parameter );
        }
        float input = m_parameter->m_input;
        float output = m_parameter->m_output;

        float minX, minY, maxX, maxY;
        GetPosition( m_minOutput, input, &minX, &minY );
        GetPosition( output, m_minInput, &maxX, &maxY );

//        glColor4f( 0.5f, 1.0f, 0.5f, 0.75f );
//        glBegin( GL_LINES );
//            glVertex2f( realX + minX, realY + minY );
//            glVertex2f( realX + maxX, realY + minY );
//
//            glVertex2f( realX + maxX, realY + minY );
//            glVertex2f( realX + maxX, realY + maxY );
//        glEnd();
//
//        glBegin( GL_LINE_LOOP );
//            glVertex2f( realX + 2, realY + minY - 9 );
//            glVertex2f( realX + 48, realY + minY - 9 );
//            glVertex2f( realX + 48, realY + minY + 5 );
//            glVertex2f( realX + 2, realY + minY + 5 );
//        glEnd();
//
//        g_renderer->TextRight( realX + minX, realY + minY, White, 10, "%2.2f", input );
//        g_renderer->TextRight( realX + maxX, realY + maxY + 10, White, 10, "%2.2f", output );

        float desiredOutput = m_parameter->m_desiredOutput;

        GetPosition( m_minOutput, input, &minX, &minY );
        GetPosition( desiredOutput, m_minInput, &maxX, &maxY );
        
        // Convert linked output visualization to modern renderer
        g_renderer->EclipseLine( realX + minX, realY + minY, realX + maxX, realY + minY, Colour(128, 255, 128, 89) );
        g_renderer->EclipseLine( realX + maxX, realY + minY, realX + maxX, realY + maxY, Colour(128, 255, 128, 89) );
    }
}


void SoundParameterGraph::Render( int realX, int realY, bool highlighted, bool clicked )
{
    InvertedBox::Render( realX, realY, false, false );

    // Removed glColor4f - color is handled by the modern renderer system
    g_renderer->TextSimple( realX + m_w/2, realY + m_h - 10, White, 16, m_parent->m_name );
    
    if( m_parameter->m_type == SoundParameter::TypeLinked )
    {
        g_renderer->TextSimple( realX + 10, realY + 10, White, 16, m_parameter->GetLinkName( m_parameter->m_link ) );
    }

    RescaleAxis     ();
    RenderAxis      ( realX, realY );
    RenderValues    ( realX, realY );
    RenderOutput    ( realX, realY );
    
    HandleMouseEvents();
}


class SoundParameterGraphScaleToggle : public InterfaceButton
{
public:
	void MouseUp() 
	{
		SoundParameterGraph *graph = (SoundParameterGraph*)m_parent->GetButton("graph");
		graph->m_linearScale = !graph->m_linearScale;
	}
};


class LinkTypeMenu : public DropDownMenu
{
    void Render( int realX, int realY, bool highlighted, bool clicked )
    {
        EclButton *paramType = m_parent->GetButton( "ParameterType" );
        DropDownMenu *menu = (DropDownMenu *) paramType;
        if( menu->GetSelectionValue() == SoundParameter::TypeLinked )
        {
            DropDownMenu::Render( realX, realY, highlighted, clicked );
        }
    }

    void MouseUp()
    {
        EclButton *paramType = m_parent->GetButton( "ParameterType" );
        DropDownMenu *menu = (DropDownMenu *) paramType;
        if( menu->GetSelectionValue() == SoundParameter::TypeLinked )
        {
            DropDownMenu::MouseUp();
        }
    }
};


class UpdateTypeMenu : public DropDownMenu
{
};


//*****************************************************************************
// Class SoundParameterEditor
//*****************************************************************************

SoundParameterEditor::SoundParameterEditor( char *_name )
:   InterfaceWindow( _name ),
    m_parameter(NULL),
    m_minOutput(0.0f),
    m_maxOutput(1.0f)
{
}


void SoundParameterEditor::Create()
{
    InterfaceWindow::Create();

    //
    // Parameter type

    DropDownMenu *paramType = new DropDownMenu(false);
    paramType->SetProperties( "ParameterType", 10, 30, 150, 18, "ParameterType", " ", false, false );
    for( int i = 0; i < SoundParameter::NumParameterTypes; ++i )
    {
        paramType->AddOption( SoundParameter::GetParameterTypeName(i) );
    }
    paramType->RegisterInt( &m_parameter->m_type );
    RegisterButton( paramType );


    //
    // Link type

    LinkTypeMenu *linkType = new LinkTypeMenu();
    linkType->SetProperties( "LinkType", 10, 50, 150, 18, "LinkType", " ", false, false );
    LList<char *> objectProperties;
    g_soundSystem->m_interface->ListProperties( &objectProperties );
    for( int i = 0; i < objectProperties.Size(); ++i )
    {
        linkType->AddOption( objectProperties[i] );
    }
    linkType->RegisterInt( &m_parameter->m_link );
    RegisterButton( linkType );


    //
    // Smoothing factor

	CreateValueControl( "SmoothFactor", 170, 30, m_w-180, 18, InputField::TypeFloat, &m_parameter->m_smooth, 0.05f, 0.0f, 0.99f, NULL, " ", false );


    //
    // Update type

    UpdateTypeMenu *updateType = new UpdateTypeMenu();
    updateType->SetProperties( "UpdateType", 170, 50, m_w - 180, 18, "UpdateType", " ", false, false );
    for( int i = 0; i < SoundParameter::NumUpdateTypes; ++i )
    {
        updateType->AddOption( SoundParameter::GetUpdateTypeName(i) );
    }
    updateType->RegisterInt( &m_parameter->m_updateType );
    RegisterButton( updateType );

    
    //
    // Graph 
    
    SoundParameterGraph *graph = new SoundParameterGraph();
    graph->SetProperties( "graph", 10, 70, m_w - 20, m_h - 80, "  ", " ", false, false );
    graph->SetParameter( m_parameter, m_minOutput, m_maxOutput );
    RegisterButton( graph );

//	SoundParameterGraphScaleToggle *scaleToggle = new SoundParameterGraphScaleToggle();
//	scaleToggle->SetShortProperties("Toggle Scale", 10, 50, 150);
//	RegisterButton(scaleToggle);
}


void SoundParameterEditor::Update()
{
    InterfaceWindow::Update();    
}
