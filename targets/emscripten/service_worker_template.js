// Search for all EDIT ME! strings to replace with your own code
// This is the template for the service worker for replay viewer and sync practice clients

// EDIT ME!
// pick your own name for the service worker

const CACHE_NAME = 'bla-bla-bla';

// EDIT ME!
// add the patterns for the files to cache

const CACHE_PATTERNS = [
    /bla-bla-bla_.*\.js$/,
    /bla-bla-bla_.*\.wasm$/,
    /bla-bla-bla_.*\.data$/,
    /bla-bla-bla_.*\.html$/,
    /bla-bla-bla\.html$/
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

                    // EDIT ME!
                    // add the patterns for the files to delete
                    
                    if (cacheName !== CACHE_NAME && cacheName.startsWith('bla-bla-bla_')) {
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

