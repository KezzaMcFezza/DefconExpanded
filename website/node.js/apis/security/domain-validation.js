const express = require('express');
const router = express.Router();
const crypto = require('crypto');
const debug = require('../../debug-helpers');
const validationAttempts = new Map();
const MAX_ATTEMPTS_PER_HOUR = 100;
const VALIDATION_WINDOW = 60 * 60 * 1000;
const AUTHORIZED_DOMAIN_HASHES = [
    3875829612, // localhost
    2320655608, // 127.0.0.1
    2982583560, // defconexpanded.com
    683682079, // www.defconexpanded.com
    2692890021, // defconexpandedhost.ddns.net
    3214606936, // www.defconexpandedhost.ddns.net
];

function isRateLimited(clientIP) {
    const startTime = debug.enter('isRateLimited', [clientIP], 2);
    
    try {
        const now = Date.now();
        
        if (!validationAttempts.has(clientIP)) {
            validationAttempts.set(clientIP, []);
            debug.level2('New client IP, creating validation attempts array:', clientIP);
        }
        
        const attempts = validationAttempts.get(clientIP);
        debug.level3('Current attempts for IP:', clientIP, 'count:', attempts.length);
        
        const validAttempts = attempts.filter(time => now - time < VALIDATION_WINDOW);
        validationAttempts.set(clientIP, validAttempts);
        
        const isLimited = validAttempts.length >= MAX_ATTEMPTS_PER_HOUR;
        debug.level2('Rate limit check for', clientIP, '- attempts:', validAttempts.length, '/', MAX_ATTEMPTS_PER_HOUR, 'limited:', isLimited);
        
        debug.exit('isRateLimited', startTime, isLimited, 2);
        return isLimited;
    } catch (error) {
        debug.error('isRateLimited', error, 1);
        debug.exit('isRateLimited', startTime, 'error', 2);
        throw error;
    }
}

function recordAttempt(clientIP) {
    const startTime = debug.enter('recordAttempt', [clientIP], 2);
    
    try {
        if (!validationAttempts.has(clientIP)) {
            validationAttempts.set(clientIP, []);
        }
        validationAttempts.get(clientIP).push(Date.now());
        
        const totalAttempts = validationAttempts.get(clientIP).length;
        debug.level2('Recorded validation attempt for', clientIP, '- total attempts:', totalAttempts);
        
        debug.exit('recordAttempt', startTime, totalAttempts, 2);
    } catch (error) {
        debug.error('recordAttempt', error, 1);
        debug.exit('recordAttempt', startTime, 'error', 2);
        throw error;
    }
}

function validateTimestamp(timestamp) {
    const startTime = debug.enter('validateTimestamp', [timestamp], 2);
    
    try {
        const now = Date.now();
        const timestampDiff = Math.abs(now - timestamp);
        const isValid = timestampDiff <= 60000;
        
        debug.level2('Timestamp validation - now:', now, 'provided:', timestamp, 'diff:', timestampDiff, 'ms, valid:', isValid);
        
        debug.exit('validateTimestamp', startTime, isValid, 2);
        return { isValid, timestampDiff };
    } catch (error) {
        debug.error('validateTimestamp', error, 1);
        debug.exit('validateTimestamp', startTime, 'error', 2);
        throw error;
    }
}

function validateDomainHash(domainHash) {
    const startTime = debug.enter('validateDomainHash', [domainHash], 2);
    
    try {
        const isValid = AUTHORIZED_DOMAIN_HASHES.includes(domainHash);
        
        debug.level2('Domain hash validation - hash:', domainHash, 'valid:', isValid);
        if (!isValid) {
            debug.level1('SECURITY ALERT: Unauthorized domain hash attempted:', domainHash);
        } else {
            debug.level2('Valid domain hash accepted:', domainHash);
        }
        
        debug.exit('validateDomainHash', startTime, isValid, 2);
        return isValid;
    } catch (error) {
        debug.error('validateDomainHash', error, 1);
        debug.exit('validateDomainHash', startTime, 'error', 2);
        throw error;
    }
}

function generateResponseChallenge(domainHash, timestamp, challenge) {
    const startTime = debug.enter('generateResponseChallenge', [domainHash, timestamp, 'challenge_hidden'], 2);
    
    try {
        const responseChallenge = crypto
            .createHash('sha256')
            .update(`${domainHash}${timestamp}${challenge}${process.env.JWT_SECRET}`)
            .digest('hex');
        
        debug.level3('Generated response challenge for domain hash:', domainHash, 'challenge length:', responseChallenge.length);
        
        debug.exit('generateResponseChallenge', startTime, 'challenge_generated', 2);
        return responseChallenge;
    } catch (error) {
        debug.error('generateResponseChallenge', error, 1);
        debug.exit('generateResponseChallenge', startTime, 'error', 2);
        throw error;
    }
}

router.post('/api/security/validate-domain', (req, res) => {
    const startTime = debug.enter('validateDomainEndpoint', [], 1);
    const clientIP = req.ip || req.connection.remoteAddress;
    
    debug.apiRequest('POST', '/api/security/validate-domain', null, 1);
    debug.level1('Domain validation request from IP:', clientIP);
    
    try {
        debug.level2('Checking rate limiting for IP:', clientIP);
        if (isRateLimited(clientIP)) {
            debug.level1('SECURITY: Rate limit exceeded for IP:', clientIP);
            debug.apiResponse(429, 'Rate limited', 1);
            debug.exit('validateDomainEndpoint', startTime, 'rate_limited', 1);
            return res.status(429).json({ 
                error: 'Too many validation attempts',
                valid: false 
            });
        }
        
        recordAttempt(clientIP);
        
        const { domainHash, timestamp, challenge } = req.body;
        debug.level2('Request parameters - domainHash:', domainHash, 'timestamp:', timestamp, 'challenge:', challenge ? 'provided' : 'missing');
        
        if (!domainHash || !timestamp || !challenge) {
            debug.level2('Missing required parameters in request from', clientIP);
            debug.apiResponse(400, 'Missing parameters', 2);
            debug.exit('validateDomainEndpoint', startTime, 'missing_params', 1);
            return res.status(400).json({ 
                error: 'Missing required parameters',
                valid: false 
            });
        }
        
        const timestampValidation = validateTimestamp(timestamp);
        if (!timestampValidation.isValid) {
            debug.level1('SECURITY: Invalid timestamp from', clientIP, '- diff:', timestampValidation.timestampDiff, 'ms');
            debug.auth('domain_validation', { ip: clientIP }, 'timestamp_invalid', 1);
            debug.apiResponse(400, 'Invalid timestamp', 2);
            debug.exit('validateDomainEndpoint', startTime, 'invalid_timestamp', 1);
            return res.status(400).json({ 
                error: 'Invalid timestamp',
                valid: false 
            });
        }
        
        const isValidDomain = validateDomainHash(domainHash);
        if (!isValidDomain) {
            debug.level1('SECURITY ALERT: Unauthorized domain access attempt from', clientIP, 'hash:', domainHash);
            debug.auth('domain_validation', { ip: clientIP, domainHash }, 'unauthorized_domain', 1);
            debug.apiResponse(403, 'Unauthorized domain', 1);
            debug.exit('validateDomainEndpoint', startTime, 'unauthorized_domain', 1);
            return res.status(403).json({ 
                error: 'Unauthorized domain',
                valid: false 
            });
        }
        
        debug.level2('Generating response challenge for valid domain');
        const responseChallenge = generateResponseChallenge(domainHash, timestamp, challenge);
        const now = Date.now();
        
        debug.level1('SECURITY: Domain validation successful for IP:', clientIP, 'hash:', domainHash);
        debug.auth('domain_validation', { ip: clientIP, domainHash }, 'success', 1);
        debug.apiResponse(200, 'Validation successful', 1);
        debug.exit('validateDomainEndpoint', startTime, 'success', 1);
        
        res.json({ 
            valid: true,
            response: responseChallenge,
            serverTime: now
        });
        
    } catch (error) {
        debug.error('validateDomainEndpoint', error, 1);
        debug.level1('CRITICAL ERROR in domain validation from', clientIP, ':', error.message);
        debug.apiResponse(500, 'Internal error', 1);
        debug.exit('validateDomainEndpoint', startTime, 'error', 1);
        
        res.status(500).json({ 
            error: 'Validation failed',
            valid: false 
        });
    }
});

router.get('/api/security/server-time', (req, res) => {
    const startTime = debug.enter('serverTimeEndpoint', [], 2);
    const clientIP = req.ip || req.connection.remoteAddress;
    
    debug.apiRequest('GET', '/api/security/server-time', null, 2);
    debug.level2('Server time request from IP:', clientIP);
    
    try {
        const serverTime = Date.now();
        const timezone = Intl.DateTimeFormat().resolvedOptions().timeZone;
        
        debug.level2('Providing server time:', serverTime, 'timezone:', timezone);
        debug.apiResponse(200, { serverTime, timezone }, 2);
        debug.exit('serverTimeEndpoint', startTime, 'success', 2);
        
        res.json({ 
            serverTime,
            timezone
        });
        
    } catch (error) {
        debug.error('serverTimeEndpoint', error, 1);
        debug.apiResponse(500, 'Error getting server time', 1);
        debug.exit('serverTimeEndpoint', startTime, 'error', 2);
        
        res.status(500).json({ 
            error: 'Failed to get server time'
        });
    }
});

setInterval(() => {
    const startTime = debug.enter('cleanupValidationAttempts', [], 3);
    
    try {
        const now = Date.now();
        let cleanedCount = 0;
        let totalCleaned = 0;
        
        for (const [ip, attempts] of validationAttempts.entries()) {
            const originalCount = attempts.length;
            const validAttempts = attempts.filter(time => now - time < VALIDATION_WINDOW);
            
            if (validAttempts.length === 0) {
                validationAttempts.delete(ip);
                cleanedCount++;
                totalCleaned += originalCount;
            } else {
                validationAttempts.set(ip, validAttempts);
                totalCleaned += (originalCount - validAttempts.length);
            }
        }
        
        if (cleanedCount > 0 || totalCleaned > 0) {
            debug.level2('Validation attempts cleanup - removed', cleanedCount, 'IPs,', totalCleaned, 'old attempts');
        }
        
        debug.exit('cleanupValidationAttempts', startTime, { cleanedCount, totalCleaned }, 3);
    } catch (error) {
        debug.error('cleanupValidationAttempts', error, 1);
        debug.exit('cleanupValidationAttempts', startTime, 'error', 3);
    }
}, VALIDATION_WINDOW); 

debug.level1('Domain validation API initialized with', AUTHORIZED_DOMAIN_HASHES.length, 'authorized domains');
debug.level2('Rate limiting: max', MAX_ATTEMPTS_PER_HOUR, 'attempts per hour');

module.exports = router; 