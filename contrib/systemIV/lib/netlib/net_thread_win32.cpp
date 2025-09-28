#include "lib/universal_include.h"
#include "lib/debug_utils.h"
#include "lib/thread_affinity.h"

#include "net_thread.h"


NetRetCode NetStartThread(NetThreadFunc functionPtr, void *arg )
{
	NetRetCode retVal = NetOk;
	DWORD dwID = 0;
	
	HANDLE hThread = CreateThread(NULL, NULL, functionPtr, arg, NULL, &dwID);
	if (hThread == NULL)
	{
		AppDebugOut("Thread creation failed");
		retVal = NetFailed;
	}
	else
	{
		// set network thread to use a different core than the main thread
		// use shared affinity settings to ensure consistency
		
		int mainCore, networkCore;
		GetThreadAffinitySettings(mainCore, networkCore);
		
		DWORD networkAffinityMask = 1 << networkCore;
		
		//
		// now set the thread affinity for the network thread

		DWORD previousAffinity = SetThreadAffinityMask(hThread, networkAffinityMask);
		if (previousAffinity == 0) {
			AppDebugOut("Warning: Failed to set network thread affinity to core %d\n", networkCore);
		} else {
			AppDebugOut("Network thread bound to core %d\n", networkCore);
		}
		
		CloseHandle(hThread);
	}

	return retVal;
}
