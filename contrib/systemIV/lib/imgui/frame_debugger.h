#ifndef IMGUI_FRAME_DEBUGGER_H
#define IMGUI_FRAME_DEBUGGER_H

#include "lib/imgui/imgui.h"

class FrameDebugger : public ImGuiWindowBase
{
private:
	bool	m_isOpen;

	static const int HISTORY_SIZE = 256;
	
	float	m_frameTimeHistory[HISTORY_SIZE];
	int		m_historyIndex;
	bool	m_historyFilled;
	float	m_lastFrameTime;

	bool	m_useFixedMax;
	float	m_fixedMaxFrameTimeMs;

	static const int TIMING_HISTORY_SIZE = 256;
	static const int MAX_TRACKED_TIMINGS = 10;

	struct TimingHistoryEntry
	{
		char name[64];
		float cpuHistory[TIMING_HISTORY_SIZE];
		float gpuHistory[TIMING_HISTORY_SIZE];
		int historyIndex;
		bool historyFilled;
	};

	TimingHistoryEntry m_timingHistory[MAX_TRACKED_TIMINGS];
	int m_timingHistoryCount;
	bool m_initialized;

	void	DrawOverview();
	void	DrawGpuTimings();
	void	DrawVboCache();
	void	DrawRuntimeControls();
	void	Window();
	
	void	UpdateGpuTimingHistory();

public:
	FrameDebugger();
	~FrameDebugger();

	void	Init() override;
	void	Shutdown() override;
	void	Update() override;
	void	Update( double frameTime );

	void	Render() override;
	bool	IsOpen() const override { return m_isOpen; }

	void	SetOpen( bool open ) { m_isOpen = open; }
	void	Toggle() { m_isOpen = !m_isOpen; }

    static FrameDebugger* GetInstance();     
};

#endif // IMGUI_FRAME_DEBUGGER_H