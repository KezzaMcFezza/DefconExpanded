#include "systemiv.h"

#include <algorithm>
#include <cstring>

#include "imgui-1.92.5/imgui.h"

#include "lib/gucci/window_manager.h"
#include "lib/render/renderer.h"
#include "lib/render2d/renderer_2d.h"
#include "lib/render3d/renderer_3d.h"
#include "lib/render2d/megavbo/megavbo_2d.h"
#include "lib/render3d/megavbo/megavbo_3d.h"
#include "lib/tosser/darray.h"
#include "lib/hi_res_time.h"

#include "frame_debugger.h"

extern Renderer *g_renderer;
extern Renderer2D *g_renderer2d;
extern Renderer3D *g_renderer3d;
extern MegaVBO2D *g_megavbo2d;
extern MegaVBO3D *g_megavbo3d;
extern WindowManager *g_windowManager;

static FrameDebugger *s_frameDebuggerInstance = nullptr;


FrameDebugger::FrameDebugger()
	: m_isOpen( false ),
	  m_historyIndex( 0 ),
	  m_historyFilled( false ),
	  m_lastFrameTime( 0.0f ),
	  m_useFixedMax( false ),
	  m_fixedMaxFrameTimeMs( 1000.0f / 240.0f ),
	  m_timingHistoryCount( 0 ),
	  m_initialized( false )
{
	SetName( "Frame Debugger" );
	s_frameDebuggerInstance = this;
}


FrameDebugger::~FrameDebugger()
{
	Shutdown();

	if ( s_frameDebuggerInstance == this )
	{
		s_frameDebuggerInstance = nullptr;
	}
}


void FrameDebugger::Init()
{

	if ( m_initialized )
		return;

	for ( int i = 0; i < HISTORY_SIZE; ++i )
	{
		m_frameTimeHistory[i] = 0.0f;
	}

	for ( int i = 0; i < MAX_TRACKED_TIMINGS; ++i )
	{
		m_timingHistory[i].name[0] = '\0';
		m_timingHistory[i].historyIndex = 0;
		m_timingHistory[i].historyFilled = false;
		for ( int j = 0; j < TIMING_HISTORY_SIZE; ++j )
		{
			m_timingHistory[i].cpuHistory[j] = 0.0f;
			m_timingHistory[i].gpuHistory[j] = 0.0f;
		}
	}

	m_initialized = true;
}


void FrameDebugger::Shutdown()
{
}


void FrameDebugger::Update()
{
	static double s_lastUpdateTime = 0.0;
	double timeNow = GetHighResTime();
	double frameTime = 0.0;

	if ( s_lastUpdateTime > 0.0 )
	{
		frameTime = timeNow - s_lastUpdateTime;
	}
	else
	{
		frameTime = 1.0 / 60.0;
	}

	s_lastUpdateTime = timeNow;

	Update( frameTime );
}


void FrameDebugger::Update( double frameTime )
{
	m_lastFrameTime = (float)frameTime;

	//
	// Store frame time in milliseconds for display

	m_frameTimeHistory[m_historyIndex] = m_lastFrameTime * 1000.0f;
	m_historyIndex = ( m_historyIndex + 1 ) % HISTORY_SIZE;

	if ( m_historyIndex == 0 )
	{
		m_historyFilled = true;
	}

	//
	// Update timing history

	if ( g_renderer )
	{
		int timingCount = 0;
		const Renderer::FlushTiming *timings = g_renderer->GetFlushTimings( timingCount );

		for ( int i = 0; i < timingCount && i < MAX_TRACKED_TIMINGS; ++i )
		{
			if ( timings[i].callCount == 0 )
				continue;

			TimingHistoryEntry *entry = nullptr;

			//
			// Find or create entry

			for ( int j = 0; j < m_timingHistoryCount; ++j )
			{
				if ( strcmp( m_timingHistory[j].name, timings[i].name ) == 0 )
				{
					entry = &m_timingHistory[j];
					break;
				}
			}

			if ( !entry && m_timingHistoryCount < MAX_TRACKED_TIMINGS )
			{
				entry = &m_timingHistory[m_timingHistoryCount];
				strncpy( entry->name, timings[i].name, sizeof( entry->name ) - 1 );
				entry->name[sizeof( entry->name ) - 1] = '\0';
				entry->historyIndex = 0;
				entry->historyFilled = false;
				m_timingHistoryCount++;
			}

			if ( entry )
			{
				float avgCpu = (float)( ( timings[i].totalTime / timings[i].callCount ) * 1000.0 );
				float avgGpu = timings[i].totalGpuTime > 0.0 ? (float)( timings[i].totalGpuTime / timings[i].callCount ) : 0.0f;

				entry->cpuHistory[entry->historyIndex] = avgCpu;
				entry->gpuHistory[entry->historyIndex] = avgGpu;
				entry->historyIndex = ( entry->historyIndex + 1 ) % TIMING_HISTORY_SIZE;

				if ( entry->historyIndex == 0 )
				{
					entry->historyFilled = true;
				}
			}
		}
	}
}


void FrameDebugger::Render()
{
	if ( !m_isOpen )
		return;

	if ( !g_renderer )
		return;

	Window();

	bool open = m_isOpen;
	if ( !ImGui::Begin( "Frame Debugger", &open ) )
	{
		ImGui::End();
		m_isOpen = open;
		return;
	}

	if ( !open )
	{
		m_isOpen = false;
	}

	if ( ImGui::BeginTabBar( "FrameDebuggerTabs" ) )
	{
		if ( ImGui::BeginTabItem( "Overview" ) )
		{
			DrawOverview();
			ImGui::EndTabItem();
		}

		if ( ImGui::BeginTabItem( "Flush Timings" ) )
		{
			DrawGpuTimings();
			ImGui::EndTabItem();
		}

		if ( ImGui::BeginTabItem( "VBO Cache" ) )
		{
			DrawVboCache();
			ImGui::EndTabItem();
		}

		if ( ImGui::BeginTabItem( "Runtime Controls" ) )
		{
			DrawRuntimeControls();
			ImGui::EndTabItem();
		}

		ImGui::EndTabBar();
	}

	ImGui::End();
}


void FrameDebugger::DrawOverview()
{
	if ( !g_renderer2d || !g_renderer3d )
	{
		ImGui::TextUnformatted( "Renderers not initialised yet." );
		return;
	}

	float frameMs = m_lastFrameTime * 1000.0f;
	float fps = ( m_lastFrameTime > 0.0f ) ? 1.0f / m_lastFrameTime : 0.0f;

	ImGui::Text( "Frame time: %.2f ms (%.1f FPS)", frameMs, fps );

	ImGui::SameLine();
	ImGui::TextDisabled( "(?)" );
	if ( ImGui::IsItemHovered() )
	{
		ImGui::SetTooltip( "Frame time is measured in the render loop.\nGraph shows newest data on the right." );
	}

	ImGui::Checkbox( "Fixed Max", &m_useFixedMax );
	ImGui::SameLine();
	if ( m_useFixedMax )
	{
		float maxFps = 1000.0f / m_fixedMaxFrameTimeMs;
		ImGui::SetNextItemWidth( 150.0f );
		if ( ImGui::SliderFloat( "##MaxFrameTime", &maxFps, 10.0f, 1000.0f, "%.0f FPS" ) )
		{
			m_fixedMaxFrameTimeMs = 1000.0f / maxFps;
		}
	}
	else
	{
		ImGui::TextDisabled( "(Auto-adjusting)" );
	}

	//
	// Frame time history

	int historyCount = m_historyFilled ? HISTORY_SIZE : m_historyIndex;
	if ( historyCount > 0 )
	{
		int offset = m_historyFilled ? m_historyIndex : 0;

		float minTime = m_frameTimeHistory[0];
		float maxTime = m_frameTimeHistory[0];
		for ( int i = 0; i < historyCount; ++i )
		{
			if ( m_frameTimeHistory[i] < minTime )
				minTime = m_frameTimeHistory[i];
			if ( m_frameTimeHistory[i] > maxTime )
				maxTime = m_frameTimeHistory[i];
		}

		float graphMax = m_useFixedMax ? m_fixedMaxFrameTimeMs : maxTime * 1.1f;

		char overlay[64];
		snprintf( overlay, sizeof( overlay ), "%.1f - %.1f ms", minTime, maxTime );

		ImVec2 availableSize = ImGui::GetContentRegionAvail();

		ImGui::PlotLines(
			"##FrameTime",
			m_frameTimeHistory,
			historyCount,
			offset,
			overlay,
			0.0f,
			graphMax,
			ImVec2( availableSize.x, 80.0f ) );
	}

	ImGui::Separator();

	int totalDrawCalls2D = g_renderer2d->GetTotalDrawCalls();
	int totalDrawCalls3D = g_renderer3d->GetTotalDrawCalls();
	int totalDrawCalls = totalDrawCalls2D + totalDrawCalls3D;

	ImGui::Text( "Total draw calls: %d (2D: %d, 3D: %d)", totalDrawCalls, totalDrawCalls2D, totalDrawCalls3D );

	ImGui::Separator();

	if ( ImGui::CollapsingHeader( "Buffer Statistics", ImGuiTreeNodeFlags_DefaultOpen ) )
	{
		if ( ImGui::BeginTable( "OverviewTable", 3, ImGuiTableFlags_BordersInnerV ) )
		{
			ImGui::TableNextColumn();
			ImGui::TextUnformatted( "2D/3D buffers" );
			ImGui::TableNextColumn();
			ImGui::TextUnformatted( "Immediate" );
			ImGui::TableNextColumn();
			ImGui::TextUnformatted( "Batched" );

			auto Row = []( const char *label, int immediate, int batched )
			{
				ImGui::TableNextRow();
				ImGui::TableNextColumn();
				ImGui::TextUnformatted( label );
				ImGui::TableNextColumn();
				ImGui::Text( "%d", immediate );
				ImGui::TableNextColumn();
				ImGui::Text( "%d", batched );
			};

			int textImmediate = g_renderer2d->GetImmediateTextCalls() + g_renderer3d->GetImmediateTextCalls();
			int textBatched = g_renderer2d->GetTextCalls() + g_renderer3d->GetTextCalls();
			Row( "Text", textImmediate, textBatched );

			int lineImmediate = g_renderer2d->GetImmediateLineCalls() + g_renderer3d->GetImmediateLineCalls();
			int lineBatched = g_renderer2d->GetLineCalls() + g_renderer3d->GetLineCalls();
			Row( "Lines", lineImmediate, lineBatched );

			int staticSpriteImmediate = g_renderer2d->GetImmediateStaticSpriteCalls() + g_renderer3d->GetImmediateStaticSpriteCalls();
			int staticSpriteBatched = g_renderer2d->GetStaticSpriteCalls() + g_renderer3d->GetStaticSpriteCalls();
			Row( "Static sprites", staticSpriteImmediate, staticSpriteBatched );

			int rotatingSpriteImmediate = g_renderer2d->GetImmediateRotatingSpriteCalls() + g_renderer3d->GetImmediateRotatingSpriteCalls();
			int rotatingSpriteBatched = g_renderer2d->GetRotatingSpriteCalls() + g_renderer3d->GetRotatingSpriteCalls();
			Row( "Rotating sprites", rotatingSpriteImmediate, rotatingSpriteBatched );

			int circleImmediate = g_renderer2d->GetImmediateCircleCalls() + g_renderer3d->GetImmediateCircleCalls();
			int circleBatched = g_renderer2d->GetCircleCalls() + g_renderer3d->GetCircleCalls();
			Row( "Circles", circleImmediate, circleBatched );

			int circleFillImmediate = g_renderer2d->GetImmediateCircleFillCalls() + g_renderer3d->GetImmediateCircleFillCalls();
			int circleFillBatched = g_renderer2d->GetCircleFillCalls() + g_renderer3d->GetCircleFillCalls();
			Row( "Circle fills", circleFillImmediate, circleFillBatched );

			int rectImmediate = g_renderer2d->GetImmediateRectCalls() + g_renderer3d->GetImmediateRectCalls();
			int rectBatched = g_renderer2d->GetRectCalls() + g_renderer3d->GetRectCalls();
			Row( "Rects", rectImmediate, rectBatched );

			int rectFillImmediate = g_renderer2d->GetImmediateRectFillCalls() + g_renderer3d->GetImmediateRectFillCalls();
			int rectFillBatched = g_renderer2d->GetRectFillCalls() + g_renderer3d->GetRectFillCalls();
			Row( "Rect fills", rectFillImmediate, rectFillBatched );

			int triFillImmediate = g_renderer2d->GetImmediateTriangleFillCalls() + g_renderer3d->GetImmediateTriangleFillCalls();
			int triFillBatched = g_renderer2d->GetTriangleFillCalls() + g_renderer3d->GetTriangleFillCalls();
			Row( "Triangle fills", triFillImmediate, triFillBatched );

			ImGui::EndTable();
		}
	}
}


void FrameDebugger::UpdateGpuTimingHistory()
{
	if ( !g_renderer )
		return;

	int timingCount = 0;
	const Renderer::FlushTiming *timings = g_renderer->GetFlushTimings( timingCount );

	//
	// Update GPU timing for existing history entries

	for ( int i = 0; i < timingCount && i < MAX_TRACKED_TIMINGS; ++i )
	{
		if ( timings[i].callCount == 0 )
			continue;

		//
		// Find existing entry in history

		TimingHistoryEntry *entry = nullptr;
		for ( int j = 0; j < m_timingHistoryCount; ++j )
		{
			if ( strcmp( m_timingHistory[j].name, timings[i].name ) == 0 )
			{
				entry = &m_timingHistory[j];
				break;
			}
		}

		if ( entry && timings[i].totalGpuTime > 0.0 )
		{
			//
			// Update the most recent GPU history entry (go back one from current index)

			int prevIndex = ( entry->historyIndex - 1 + TIMING_HISTORY_SIZE ) % TIMING_HISTORY_SIZE;
			float avgGpu = (float)( timings[i].totalGpuTime / timings[i].callCount );
			entry->gpuHistory[prevIndex] = avgGpu;
		}
	}
}


void FrameDebugger::DrawGpuTimings()
{
	if ( !g_renderer )
	{
		ImGui::TextUnformatted( "Renderer not initialised." );
		return;
	}

	g_renderer->UpdateGpuTimings();

	//
	// Update GPU timing history now that GPU data is available

	UpdateGpuTimingHistory();

	int timingCount = 0;
	const Renderer::FlushTiming *timings = g_renderer->GetFlushTimings( timingCount );

	if ( timingCount == 0 || !timings )
	{
		ImGui::TextUnformatted( "No GPU timings recorded yet." );
		return;
	}

	const int maxShown = 16;

	//
	// Copy into a local array we can sort

	static Renderer::FlushTiming s_sorted[64];
	int copyCount = std::min( timingCount, (int)( sizeof( s_sorted ) / sizeof( s_sorted[0] ) ) );

	for ( int i = 0; i < copyCount; ++i )
	{
		s_sorted[i] = timings[i];
	}

	std::sort(
		s_sorted,
		s_sorted + copyCount,
		[]( const Renderer::FlushTiming &a, const Renderer::FlushTiming &b )
		{
			return a.totalTime > b.totalTime;
		} );

	ImGui::TextUnformatted( "Top flush timings (CPU / GPU per call)" );
	ImGui::SameLine();
	ImGui::TextDisabled( "(?)" );
	if ( ImGui::IsItemHovered() )
	{
		ImGui::SetTooltip( "Red GPU times indicate GPU-bound operations.\nGreen indicates CPU-bound.\nN/A means GPU timing unavailable on this platform." );
	}
	ImGui::Separator();

	if ( ImGui::BeginTable( "GpuTimingsTable", 4, ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersInnerV ) )
	{
		ImGui::TableSetupColumn( "Name" );
		ImGui::TableSetupColumn( "CPU ms" );
		ImGui::TableSetupColumn( "GPU ms" );
		ImGui::TableSetupColumn( "Calls" );
		ImGui::TableHeadersRow();

		int shown = 0;
		for ( int i = 0; i < copyCount && shown < maxShown; ++i )
		{
			if ( s_sorted[i].callCount == 0 )
				continue;

			double avgCpuMs = ( s_sorted[i].totalTime / s_sorted[i].callCount ) * 1000.0;
			double avgGpuMs = s_sorted[i].totalGpuTime > 0.0 ? ( s_sorted[i].totalGpuTime / s_sorted[i].callCount ) : 0.0;

			ImGui::TableNextRow();

			ImGui::TableNextColumn();
			ImGui::TextUnformatted( s_sorted[i].name );

			ImGui::TableNextColumn();
			ImGui::Text( "%.3f", avgCpuMs );

			ImGui::TableNextColumn();

			if ( s_sorted[i].totalGpuTime > 0.0 )
			{
				if ( avgGpuMs > avgCpuMs )
				{
					ImGui::TextColored( ImVec4( 1.0f, 0.4f, 0.4f, 1.0f ), "%.3f", avgGpuMs );
				}
				else
				{
					ImGui::TextColored( ImVec4( 0.4f, 1.0f, 0.4f, 1.0f ), "%.3f", avgGpuMs );
				}
			}
			else
			{
				ImGui::TextColored( ImVec4( 0.6f, 0.6f, 0.6f, 1.0f ), "N/A" );
			}

			ImGui::TableNextColumn();
			ImGui::Text( "%d", s_sorted[i].callCount );

			++shown;
		}

		ImGui::EndTable();
	}

	ImGui::Separator();
	ImGui::TextUnformatted( "Timing History Graphs" );
	ImGui::SameLine();
	ImGui::TextDisabled( "(?)" );
	if ( ImGui::IsItemHovered() )
	{
		ImGui::SetTooltip( "Expand individual graphs to view timing history.\nGreen = CPU time, Orange = GPU time" );
	}

	if ( m_timingHistoryCount == 0 )
	{
		ImGui::TextDisabled( "No timing data collected yet." );
		return;
	}

	//
	// Draw graphs for each tracked timing

	for ( int t = 0; t < m_timingHistoryCount; ++t )
	{
		TimingHistoryEntry *entry = &m_timingHistory[t];
		int historyCount = entry->historyFilled ? TIMING_HISTORY_SIZE : entry->historyIndex;

		if ( historyCount == 0 )
			continue;

		//
		// Get latest values for header display

		int latestIdx = entry->historyIndex > 0 ? entry->historyIndex - 1 : ( entry->historyFilled ? TIMING_HISTORY_SIZE - 1 : 0 );
		float latestCpu = entry->cpuHistory[latestIdx];
		float latestGpu = entry->gpuHistory[latestIdx];

		//
		// Check if GPU timing is available

		bool hasGpuTiming = latestGpu > 0.0f;

		if ( !hasGpuTiming )
		{
			//
			// Also check historical values in case latest is zero due to timing

			for ( int i = 0; i < historyCount; ++i )
			{
				if ( entry->gpuHistory[i] > 0.0f )
				{
					hasGpuTiming = true;
					break;
				}
			}
		}

		char headerLabel[128];
		char gpuText[32];
		if ( hasGpuTiming )
		{
			snprintf( gpuText, sizeof( gpuText ), "%.3f ms", latestGpu );
		}
		else
		{
			snprintf( gpuText, sizeof( gpuText ), "N/A    " );
		}
		snprintf( headerLabel, sizeof( headerLabel ), "%s##%s", entry->name, entry->name );

		if ( !ImGui::CollapsingHeader( headerLabel ) )
			continue;

		//
		// Show stats below header

		ImGui::TextDisabled( "CPU: %.3f ms | GPU: %s", latestCpu, gpuText );

		int offset = entry->historyFilled ? entry->historyIndex : 0;

		//
		// Calculate min/max for both CPU and GPU

		float minTime = entry->cpuHistory[0];
		float maxTime = entry->cpuHistory[0];

		for ( int i = 0; i < historyCount; ++i )
		{
			if ( entry->cpuHistory[i] < minTime )
				minTime = entry->cpuHistory[i];
			if ( entry->cpuHistory[i] > maxTime )
				maxTime = entry->cpuHistory[i];

			if ( entry->gpuHistory[i] > 0.0f )
			{
				if ( entry->gpuHistory[i] < minTime )
					minTime = entry->gpuHistory[i];
				if ( entry->gpuHistory[i] > maxTime )
					maxTime = entry->gpuHistory[i];
			}
		}

		ImVec2 availableSize = ImGui::GetContentRegionAvail();
		ImVec2 graphSize( availableSize.x, 100.0f );
		float scaleMax = maxTime * 1.1f;

		//
		// Draw custom graph with two lines

		ImDrawList *drawList = ImGui::GetWindowDrawList();
		ImVec2 graphPos = ImGui::GetCursorScreenPos();

		char buttonId[128];
		snprintf( buttonId, sizeof( buttonId ), "##graph_%s", entry->name );
		ImGui::InvisibleButton( buttonId, graphSize );

		//
		// Background

		drawList->AddRectFilled( graphPos, ImVec2( graphPos.x + graphSize.x, graphPos.y + graphSize.y ),
								 ImGui::GetColorU32( ImGuiCol_FrameBg ) );

		//
		// Draw grid lines

		for ( int i = 1; i < 5; ++i )
		{
			float y = graphPos.y + ( graphSize.y * i / 5.0f );
			drawList->AddLine( ImVec2( graphPos.x, y ), ImVec2( graphPos.x + graphSize.x, y ),
							   ImGui::GetColorU32( ImGuiCol_TextDisabled, 0.3f ) );
		}

		//
		// Draw CPU line (green)

		for ( int i = 1; i < historyCount; ++i )
		{
			int idx0 = ( offset + i - 1 ) % TIMING_HISTORY_SIZE;
			int idx1 = ( offset + i ) % TIMING_HISTORY_SIZE;

			float x0 = graphPos.x + ( graphSize.x * ( i - 1 ) / (float)( historyCount - 1 ) );
			float x1 = graphPos.x + ( graphSize.x * i / (float)( historyCount - 1 ) );

			float y0 = graphPos.y + graphSize.y - ( entry->cpuHistory[idx0] / scaleMax * graphSize.y );
			float y1 = graphPos.y + graphSize.y - ( entry->cpuHistory[idx1] / scaleMax * graphSize.y );

			ImU32 cpuColor = IM_COL32( 0, 255, 0, 255 );

			drawList->AddLine( ImVec2( x0, y0 ), ImVec2( x1, y1 ), cpuColor, 2.0f );
		}

		//
		// Draw GPU line (orange)

		if ( hasGpuTiming )
		{
			for ( int i = 1; i < historyCount; ++i )
			{
				int idx0 = ( offset + i - 1 ) % TIMING_HISTORY_SIZE;
				int idx1 = ( offset + i ) % TIMING_HISTORY_SIZE;

				float x0 = graphPos.x + ( graphSize.x * ( i - 1 ) / (float)( historyCount - 1 ) );
				float x1 = graphPos.x + ( graphSize.x * i / (float)( historyCount - 1 ) );

				float y0 = graphPos.y + graphSize.y - ( entry->gpuHistory[idx0] / scaleMax * graphSize.y );
				float y1 = graphPos.y + graphSize.y - ( entry->gpuHistory[idx1] / scaleMax * graphSize.y );

				ImU32 gpuColor = IM_COL32( 255, 165, 0, 255 );

				drawList->AddLine( ImVec2( x0, y0 ), ImVec2( x1, y1 ), gpuColor, 2.0f );
			}
		}

		//
		// Border

		drawList->AddRect( graphPos, ImVec2( graphPos.x + graphSize.x, graphPos.y + graphSize.y ),
						   ImGui::GetColorU32( ImGuiCol_Border ) );

		//
		// Legend on one line

		ImGui::TextColored( ImVec4( 0.0f, 1.0f, 0.0f, 1.0f ), "CPU" );
		ImGui::SameLine();
		ImGui::TextUnformatted( "(green)" );
		ImGui::SameLine();
		if ( hasGpuTiming )
		{
			ImGui::TextColored( ImVec4( 1.0f, 0.65f, 0.0f, 1.0f ), "GPU" );
			ImGui::SameLine();
			ImGui::TextUnformatted( "(orange)" );
			ImGui::SameLine();
		}
		ImGui::TextDisabled( "Range: %.3f - %.3f ms", minTime, maxTime );
	}
}


void FrameDebugger::DrawVboCache()
{
	if ( !g_megavbo2d || !g_megavbo3d )
	{
		ImGui::TextUnformatted( "MegaVBO cache not initialised." );
		return;
	}

	int cached2DVBOCount = g_megavbo2d->GetCachedVBOCount();
	int cached3DVBOCount = g_megavbo3d->GetCached3DVBOCount();
	int totalCachedVBOs = cached2DVBOCount + cached3DVBOCount;

	ImGui::Text( "Total cached VBOs: %d (2D: %d, 3D: %d)", totalCachedVBOs, cached2DVBOCount, cached3DVBOCount );

	if ( totalCachedVBOs == 0 )
	{
		ImGui::Separator();
		ImGui::TextWrapped( "No VBOs cached yet. VBOs are cached after first use." );
		return;
	}

	ImGui::Separator();

	auto RenderVboDropdown = []( const char *label, const char *key, int vertices, int indices, size_t vertexSize )
	{
		if ( vertices <= 0 )
			return;

		float vboMB = ( ( vertices * vertexSize ) + ( indices * sizeof( unsigned int ) ) ) / ( 1024.0f * 1024.0f );

		ImVec4 colour = ImVec4( 0.8f, 0.8f, 0.8f, 1.0f );

		char headerLabel[512];
		snprintf( headerLabel, sizeof( headerLabel ), "%s##%s", label, key );

		if ( !ImGui::CollapsingHeader( headerLabel ) )
			return;

		ImGui::Text( "Vertices: %d", vertices );
		ImGui::Text( "Indices: %d", indices );
		ImGui::Text( "Memory: " );
		ImGui::SameLine();
		ImGui::TextColored( colour, "%.2f MB", vboMB );
	};

	DArray<std::string> *keys2D = g_megavbo2d->GetAllCachedVBOKeys();
	for ( int i = 0; i < keys2D->Size(); ++i )
	{
		const std::string &key = keys2D->GetData( i );
		int vertices = g_megavbo2d->GetCachedVBOVertexCount( key.c_str() );
		int indices = g_megavbo2d->GetCachedVBOIndexCount( key.c_str() );
		size_t vertexSize = g_megavbo2d->GetCachedVBOVertexSize( key.c_str() );

		char label[512];
		snprintf( label, sizeof( label ), "2D: %s", key.c_str() );
		RenderVboDropdown( label, key.c_str(), vertices, indices, vertexSize );
	}
	delete keys2D;

	DArray<std::string> *keys3D = g_megavbo3d->GetAllCached3DVBOKeys();
	for ( int i = 0; i < keys3D->Size(); ++i )
	{
		const std::string &key = keys3D->GetData( i );
		int vertices = g_megavbo3d->GetCached3DVBOVertexCount( key.c_str() );
		int indices = g_megavbo3d->GetCached3DVBOIndexCount( key.c_str() );
		size_t vertexSize = g_megavbo3d->GetCached3DVBOVertexSize( key.c_str() );

		char label[512];
		snprintf( label, sizeof( label ), "3D: %s", key.c_str() );
		RenderVboDropdown( label, key.c_str(), vertices, indices, vertexSize );
	}
	delete keys3D;
}


void FrameDebugger::DrawRuntimeControls()
{
	if ( !g_renderer2d || !g_renderer3d )
	{
		return;
	}

	bool immediate2D = g_renderer2d->IsImmediateModeEnabled();
	bool immediate3D = g_renderer3d->IsImmediateModeEnabled3D();

	if ( ImGui::Checkbox( "Immediate Mode 2D", &immediate2D ) )
	{
		g_renderer2d->SetImmediateModeEnabled( immediate2D );
	}

	if ( ImGui::Checkbox( "Immediate Mode 3D", &immediate3D ) )
	{
		g_renderer3d->SetImmediateModeEnabled3D( immediate3D );
	}
}

void FrameDebugger::Window()
{
	ImGuiSetWindowLayout(
		FRAME_DEBUGGER_DEFAULT_X_PERCENT,
		FRAME_DEBUGGER_DEFAULT_Y_PERCENT,
		FRAME_DEBUGGER_DEFAULT_WIDTH_PERCENT,
		FRAME_DEBUGGER_DEFAULT_HEIGHT_PERCENT );
}

FrameDebugger *FrameDebugger::GetInstance()
{
	return s_frameDebuggerInstance;
}