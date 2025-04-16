//DefconExpanded, Created by...
//KezzaMcFezza - Main Developer
//Nexustini - Server Managment
//
//Notable Mentions...
//Rad - For helping with python scripts.
//Bert_the_turtle - Doing everthing with c++
//
//Inspired by Sievert and Wan May
// 
//Last Edited 16-04-2025

const dataCache = {};

export async function fetchData(graphType, options = {}) {
    const { byServer = false, playerName, startDate, endDate, skipCache = false } = options;

    const cacheKey = `${graphType}_${playerName || ''}_${startDate || ''}_${endDate || ''}`;

    if (!skipCache && dataCache[cacheKey]) {
        return dataCache[cacheKey];
    }

    try {
        const queryParams = new URLSearchParams({
            graphType
        });

        if (byServer) {
            queryParams.append('byServer', 'true');
        }

        if (playerName) {
            queryParams.append('playerName', playerName);
        }

        if (startDate) {
            queryParams.append('startDate', startDate);
        }
        if (endDate) {
            queryParams.append('endDate', endDate);
        }

        const response = await fetch(`/api/games-timeline?${queryParams.toString()}`);
        const data = await response.json();

        if (!data || data.length === 0) {
            console.warn(`No data returned for query: ${queryParams.toString()}`);
            return null;
        }

        dataCache[cacheKey] = data;
        return data;
    } catch (error) {
        console.error(`Error fetching ${graphType} data:`, error);
        return null;
    }
}

export async function fetchServerCounts(graphType, serverTypes, filters = {}) {
    const data = await fetchData(graphType, {
        playerName: filters.playerName,
        startDate: filters.startDate,
        endDate: filters.endDate
    });

    if (!data) return null;

    const serverCounts = Object.fromEntries(
        serverTypes.map(serverType => [serverType, 0])
    );

    data.forEach(day => {
        serverTypes.forEach(serverType => {
            serverCounts[serverType] += day[serverType] || 0;
        });
    });

    return serverCounts;
}

export function clearDataCache() {
    Object.keys(dataCache).forEach(key => {
        delete dataCache[key];
    });
}