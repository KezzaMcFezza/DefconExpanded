#ifndef DOMAIN_PROTECTION_H
#define DOMAIN_PROTECTION_H

#ifdef TARGET_EMSCRIPTEN
#include <emscripten.h>

class DomainProtection {
public:
    // Check if the game is running on an authorized domain
    static bool IsAuthorizedDomain();
    
    // Validate domain with obfuscated check
    static bool ValidateDomainObfuscated();
    
    // Continuous validation (call periodically)
    static bool ContinuousValidation();
    
    // Get domain hash for server validation
    static unsigned long GetDomainHash();
    
    // Destroy game if unauthorized
    static void DestroyUnauthorizedGame();

private:
    static bool s_validated;
    static int s_validationCount;
    static double s_lastValidationTime;
    
    // Obfuscated domain checking
    static bool CheckDomainInternal();
    static unsigned long HashString(const char* str);
};

#endif // TARGET_EMSCRIPTEN

#endif // DOMAIN_PROTECTION_H 