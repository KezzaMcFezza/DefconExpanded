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

const CACHE_NAME = 'replay-viewer-cache';

const CACHE_PATTERNS = [
    /replay_viewer_.*\.js$/,
    /replay_viewer_.*\.wasm$/,
    /replay_viewer_.*\.data$/,
    /replay_viewer_.*\.html$/,
    /replay-viewer\.html$/
];

self.addEventListener('install', event => {
    event.waitUntil(
        caches.open(CACHE_NAME)
            .then(cache => {
                return self.skipWaiting(); 
            })
            .catch(error => {
                throw error;
            })
    );
});

self.addEventListener('activate', event => {
    event.waitUntil(
        caches.keys().then(cacheNames => {
            return Promise.all(
                cacheNames.map(cacheName => {
                    if (cacheName !== CACHE_NAME && cacheName.startsWith('replay_viewer_')) {
                        return caches.delete(cacheName);
                    }
                })
            );
        }).then(() => {
            return self.clients.claim();
        })
    );
});

self.addEventListener('fetch', event => {
    const url = new URL(event.request.url);
    
    const shouldCache = CACHE_PATTERNS.some(pattern => pattern.test(url.pathname));
    
    if (shouldCache) {
        event.respondWith(
            caches.match(event.request)
                .then(response => {
                    if (response) {
                        return response;
                    }
                    
                    return fetch(event.request)
                        .then(response => {
                            if (!response || response.status !== 200 || response.type !== 'basic') {
                                return response;
                            }
                            
                            const responseToCache = response.clone();
                            
                            caches.open(CACHE_NAME)
                                .then(cache => {
                                    cache.put(event.request, responseToCache);
                                });
                            
                            return response;
                        })
                        .catch(error => {
                            throw error;
                        });
                })
        );
    }
});

self.addEventListener('message', event => {
    if (event.data && event.data.type === 'SKIP_WAITING') {
        self.skipWaiting();
    }
}); 

