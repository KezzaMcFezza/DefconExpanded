#include "lib/universal_include.h"

#ifdef TARGET_EMSCRIPTEN

#include "domain_protection.h"
#include "lib/hi_res_time.h"
#include <emscripten.h>
#include <cstring>
#include <cstdlib>

// Static member initialization
bool DomainProtection::s_validated = false;
int DomainProtection::s_validationCount = 0;
double DomainProtection::s_lastValidationTime = 0.0;

// Obfuscated authorized domain hashes
static const unsigned long AUTHORIZED_HASHES[] = {
    0xE704776CU,  // Hash of localhost
    0x8A5264F8U,  // Hash of 127.0.0.1
    0xB1C69D08U,  // Hash of defconexpanded.com
    0x28C0291FU,  // Hash of www.defconexpanded.com
    0xA0823DA5U,  // Hash of defconexpandedhost.ddns.net
    0xBF9B0258U,  // Hash of www.defconexpandedhost.ddns.net
    0x00000000U   // Terminator
};

// XOR key for obfuscation unique for DefconExpanded
static const unsigned long XOR_KEY = 0xD10ADAE2;

unsigned long DomainProtection::HashString(const char* str) {
    if (!str) return 0;
    
    unsigned long hash = 5381;
    while (*str) {
        hash = ((hash << 5) + hash) + *str++;
    }
    return hash ^ XOR_KEY;
}

EM_JS(char*, get_current_domain, (), {
    try {
        // Use hostname instead of host to exclude port numbers
        var domain = window.location.hostname.toLowerCase();
        
        var length = lengthBytesUTF8(domain) + 1;
        var buffer = _malloc(length);
        stringToUTF8(domain, buffer, length);
        return buffer;
    } catch(e) {
        console.error('Domain Protection Error:', e);
        return 0;
    }
});

EM_JS(char*, get_current_protocol, (), {
    try {
        var protocol = window.location.protocol;
        var length = lengthBytesUTF8(protocol) + 1;
        var buffer = _malloc(length);
        stringToUTF8(protocol, buffer, length);
        return buffer;
    } catch(e) {
        return 0;
    }
});

EM_JS(bool, check_referrer_validation, (), {
    try {
        var referrer = document.referrer.toLowerCase();
        var currentDomain = window.location.hostname.toLowerCase();
        
        if (!referrer || referrer.includes(currentDomain)) {
            return true;
        }
        
        var authorizedReferrers = [
            'defconexpanded.com',
            'www.defconexpanded.com',
            'defconexpandedhost.ddns.net',
            'www.defconexpandedhost.ddns.net',
            'group6.info',
            'www.group6.info',
            'sfcon.demoszenen.de',
            'www.sfcon.demoszenen.de'
        ];
        
        return authorizedReferrers.some(domain => referrer.includes(domain));
    } catch(e) {
        return false;
    }
});

EM_JS(bool, check_iframe_protection, (), {
    try {
        // Prevent the game from running inside unauthorized iframes
        return window.self === window.top;
    } catch(e) {
        // If we can't check, assume we're in an iframe
        return false;
    }
});

bool DomainProtection::CheckDomainInternal() {
    char* domain = get_current_domain();
    char* protocol = get_current_protocol();
    
    if (!domain || !protocol) {
        if (domain) free(domain);
        if (protocol) free(protocol);
        return false;
    }
    
    // Check protocol (must be HTTPS in production, allow HTTP for localhost)
    bool validProtocol = false;
    if (strcmp(protocol, "https:") == 0) {
        validProtocol = true;
    } else if (strcmp(protocol, "http:") == 0 && 
               (strcmp(domain, "localhost") == 0 || strcmp(domain, "127.0.0.1") == 0)) {
        validProtocol = true;
    }
    
    free(protocol);
    
    if (!validProtocol) {
        free(domain);
        return false;
    }
    
    // Check domain against authorized hashes
    unsigned long domainHash = HashString(domain);
    
    free(domain);
    
    for (int i = 0; AUTHORIZED_HASHES[i] != 0; i++) {
        if (domainHash == AUTHORIZED_HASHES[i]) {
            return true;
        }
    }
    
    return false;
}

bool DomainProtection::IsAuthorizedDomain() {
    return CheckDomainInternal() && 
           check_referrer_validation() && 
           check_iframe_protection();
}

bool DomainProtection::ValidateDomainObfuscated() {
    // Multiple validation layers with obfuscation
    double currentTime = GetHighResTime();
    
    // Add some randomness to make it harder to bypass
    if ((s_validationCount % 7) == 0) {
        if (!IsAuthorizedDomain()) {
            return false;
        }
    }
    
    // Time-based validation
    if (currentTime - s_lastValidationTime > 30.0) { // Validate every 30 seconds
        s_lastValidationTime = currentTime;
        if (!IsAuthorizedDomain()) {
            return false;
        }
    }
    
    s_validationCount++;
    s_validated = true;
    return true;
}

bool DomainProtection::ContinuousValidation() {
    // Call this from your main loop
    static int callCount = 0;
    callCount++;
    
    // Validate every 1000 calls (roughly every 4-5 seconds at 240fps)
    if ((callCount % 1000) == 0) {
        if (!ValidateDomainObfuscated()) {
            DestroyUnauthorizedGame();
            return false;
        }
    }
    
    return true;
}

unsigned long DomainProtection::GetDomainHash() {
    char* domain = get_current_domain();
    if (!domain) return 0;
    
    unsigned long hash = HashString(domain);
    free(domain);
    return hash;
}

void DomainProtection::DestroyUnauthorizedGame() {
    // Multiple destruction methods to make bypassing harder
    
    // Method 1: Crash the WebAssembly module and display unauthorized message
    EM_ASM({
        console.clear();
        try {
            // Overwrite critical game functions
            if (typeof Module !== 'undefined') {
                Module._main = function() { return -1; };
                Module.canvas = null;
                Module.ctx = null;
                Module.abort = function() {};
            }
            
            // Clear the canvas
            var canvas = document.querySelector('canvas');
            if (canvas) {
                var ctx = canvas.getContext('2d') || canvas.getContext('webgl') || canvas.getContext('webgl2');
                if (ctx) {
                    if (ctx.clearColor) {
                        ctx.clearColor(0, 0, 0, 1);
                        ctx.clear(ctx.COLOR_BUFFER_BIT);
                    } else {
                        ctx.fillStyle = 'black';
                        ctx.fillRect(0, 0, canvas.width, canvas.height);
                    }
                }
                canvas.style.display = 'none';
            }
            
            // Display unauthorized message
            document.body.innerHTML = '<div style="position:fixed;top:0;left:0;width:100%;height:100%;background:black;color:red;font-size:24px;text-align:center;padding-top:40%;z-index:9999;font-family:monospace;">UNAUTHORIZED ACCESS DETECTED<br/><br/>This content is protected by copyright.<br/>Unauthorized distribution is prohibited.</div>';
            
            // Remove all scripts to prevent further execution
            var scripts = document.querySelectorAll('script');
            scripts.forEach(function(script) {
                script.remove();
            });
            
        } catch(e) {}
    });
    
    // Method 2: Force exit via Emscripten
    emscripten_force_exit(1);
}

#endif // TARGET_EMSCRIPTEN 