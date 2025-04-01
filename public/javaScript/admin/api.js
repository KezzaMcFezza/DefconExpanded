
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
//Last Edited 01-04-2025


import UI from './ui.js';

const API = (() => {
    async function fetchWithErrorHandling(url, options = {}) {
        try {
            const defaultOptions = {
                credentials: 'include',
                headers: {}
            };
            
            
            const mergedOptions = {
                ...defaultOptions,
                ...options,
                headers: {
                    ...defaultOptions.headers,
                    ...(options.headers || {})
                }
            };
            
            const response = await fetch(url, mergedOptions);
            
            if (!response.ok) {
                const errorData = await response.json().catch(() => ({}));
                throw new Error(errorData.error || `HTTP error! status: ${response.status}`);
            }
            
            const contentType = response.headers.get("content-type");
            if (contentType && contentType.includes("application/json")) {
                return await response.json();
            }
            
            return await response.text();
        } catch (error) {
            console.error(`API Error (${url}):`, error);
            throw error;
        }
    }
    
    
    async function get(endpoint) {
        return fetchWithErrorHandling(endpoint);
    }
    
    async function post(endpoint, data, isFormData = false) {
        const options = {
            method: 'POST'
        };
        
        if (isFormData) {
            options.body = data;
        } else {
            options.headers = {
                'Content-Type': 'application/json'
            };
            options.body = JSON.stringify(data);
        }
        
        return fetchWithErrorHandling(endpoint, options);
    }
    
    async function put(endpoint, data, isFormData = false) {
        const options = {
            method: 'PUT'
        };
        
        if (isFormData) {
            options.body = data;
        } else {
            options.headers = {
                'Content-Type': 'application/json'
            };
            options.body = JSON.stringify(data);
        }
        
        return fetchWithErrorHandling(endpoint, options);
    }
    
    async function del(endpoint) {
        return fetchWithErrorHandling(endpoint, { method: 'DELETE' });
    }
    
    return {
        fetchWithErrorHandling,
        get,
        post,
        put,
        del
    };
})();

export default API;