#ifndef __INCLUDED_THREAD_AFFINITY_H
#define __INCLUDED_THREAD_AFFINITY_H

#include <thread>

//
// thread affinity configuration
// ensures main thread and network threads dont compete for the same core

inline void GetThreadAffinitySettings(int &mainCore, int &networkCore)
{
    int numCores = std::thread::hardware_concurrency();
    
    if (numCores <= 4) {

        //
        // on quad-core systems, use cores 3 and 4

        mainCore = numCores - 2;      
        networkCore = numCores - 1;   
    } else if (numCores <= 8) {

        //
        // on 6-8 core systems, use cores 5 and 6

        mainCore = numCores - 2;      
        networkCore = numCores - 1;  
    } else if (numCores >= 10) {

        //
        // on 10 core systems and above, use core 6 on the first CCD

        mainCore = 6;                 
        networkCore = 7;              
    }
}


#endif // __INCLUDED_THREAD_AFFINITY_H
