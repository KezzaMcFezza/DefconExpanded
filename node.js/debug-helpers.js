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
//Last Edited 25-05-2025

const debugUtils = require('./debug-utils');

const debug = {
    enter: (functionName, args = [], level = 1) => {
        return debugUtils.debugFunctionEntry(functionName, args, level);
    },

    exit: (functionName, startTime, result = null, level = 1) => {
        debugUtils.debugFunctionExit(functionName, startTime, result, level);
    },

    dbQuery: (query, params = [], level = 2) => {
        console.debug(level, `DB Query: ${query.substring(0, 100)}${query.length > 100 ? '...' : ''}`, 
                     params.length > 0 ? `with ${params.length} params` : '');
    },

    dbResult: (result, level = 2) => {
        if (Array.isArray(result)) {
            console.debug(level, `DB Result: ${result.length} rows returned`);
        } else {
            console.debug(level, `DB Result:`, result);
        }
    },

    apiRequest: (method, url, user = null, level = 1) => {
        const userInfo = user ? `by ${user.username || user.id}` : 'anonymous';
        console.debug(level, `API ${method} ${url} ${userInfo}`);
    },

    apiResponse: (statusCode, data = null, level = 2) => {
        const dataInfo = data ? `with ${JSON.stringify(data).length} chars` : 'no data';
        console.debug(level, `API Response: ${statusCode} ${dataInfo}`);
    },

    error: (functionName, error, level = 1) => {
        console.debug(level, `ERROR in ${functionName}: ${error.message}`);
        if (level >= 2) {
            console.debug(2, `Error stack: ${error.stack}`);
        }
    },

    fileOp: (operation, filePath, level = 2) => {
        console.debug(level, `File ${operation}: ${filePath}`);
    },

    auth: (action, user = null, result = null, level = 2) => {
        const userInfo = user ? `user ${user.username || user.id}` : 'unknown user';
        const resultInfo = result ? ` - ${result}` : '';
        console.debug(level, `Auth ${action}: ${userInfo}${resultInfo}`);
    },

    permission: (action, user, permission, granted, level = 2) => {
        const status = granted ? 'GRANTED' : 'DENIED';
        console.debug(level, `Permission ${action}: ${user.username} - ${permission} - ${status}`);
    },

    performance: (operation, duration, level = 2) => {
        console.debug(level, `Performance: ${operation} took ${duration}ms`);
    },

    level1: (...args) => console.debug(1, ...args),
    level2: (...args) => console.debug(2, ...args),
    level3: (...args) => console.debug(3, ...args),

    if: (condition, level, ...args) => {
        if (condition) {
            console.debug(level, ...args);
        }
    },

    inspect: (obj, name = 'Object', level = 3) => {
        console.debug(level, `${name}:`, JSON.stringify(obj, null, 2).substring(0, 500));
    },

    collection: (collection, name = 'Collection', level = 2) => {
        if (Array.isArray(collection)) {
            console.debug(level, `${name}: Array with ${collection.length} items`);
        } else if (collection instanceof Map) {
            console.debug(level, `${name}: Map with ${collection.size} entries`);
        } else if (collection instanceof Set) {
            console.debug(level, `${name}: Set with ${collection.size} items`);
        } else {
            console.debug(level, `${name}: ${typeof collection}`);
        }
    },

    time: (label) => {
        const startTime = Date.now();
        return {
            end: (level = 2) => {
                const duration = Date.now() - startTime;
                console.debug(level, `Timer ${label}: ${duration}ms`);
                return duration;
            }
        };
    }
};

debug.wrapAsync = (functionName, asyncFn, level = 1) => {
    return async (...args) => {
        const startTime = debug.enter(functionName, args, level);
        try {
            const result = await asyncFn(...args);
            debug.exit(functionName, startTime, result, level);
            return result;
        } catch (error) {
            debug.error(functionName, error, level);
            debug.exit(functionName, startTime, 'error', level);
            throw error;
        }
    };
};

debug.wrapSync = (functionName, syncFn, level = 1) => {
    return (...args) => {
        const startTime = debug.enter(functionName, args, level);
        try {
            const result = syncFn(...args);
            debug.exit(functionName, startTime, result, level);
            return result;
        } catch (error) {
            debug.error(functionName, error, level);
            debug.exit(functionName, startTime, 'error', level);
            throw error;
        }
    };
};

module.exports = debug; 