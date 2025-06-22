#!/usr/bin/env node

/**
 * Domain Hash Generator for DEFCON WebAssembly Protection
 * 
 * This script generates the domain hashes that need to be added to both:
 * 1. source/lib/domain_protection.cpp (AUTHORIZED_HASHES array)
 * 2. website/node.js/apis/security/domain-validation.js (AUTHORIZED_DOMAIN_HASHES array)
 * 
 * Usage: node tools/generate-domain-hashes.js your-domain.com www.your-domain.com
 */

const XOR_KEY = 0xD10ADAE2; // Must match your C++ and Node.js code

function hashString(str) {
    if (!str) return 0;
    
    let hash = 5381;
    for (let i = 0; i < str.length; i++) {
        hash = ((hash << 5) + hash) + str.charCodeAt(i);
    }
    return (hash ^ XOR_KEY) >>> 0; // Ensure unsigned 32-bit
}

function formatHexHash(hash) {
    return '0x' + hash.toString(16).toUpperCase().padStart(8, '0') + 'U';
}

console.log('='.repeat(60));
console.log('DEFCON WEBASSEMBLY DOMAIN HASH GENERATOR');
console.log('='.repeat(60));
console.log();

const domains = process.argv.slice(2);

if (domains.length === 0) {
    console.log('Usage: node generate-domain-hashes.js domain1.com domain2.com ...');
    console.log();
    console.log('Example:');
    console.log('  node generate-domain-hashes.js your-website.com www.your-website.com');
    console.log();
    process.exit(1);
}

// Always include localhost for development
const allDomains = ['localhost', '127.0.0.1', ...domains];

console.log('Generating hashes for domains:');
allDomains.forEach(domain => {
    console.log(`  - ${domain}`);
});
console.log();

console.log('Generated Hashes:');
console.log('-'.repeat(30));

const hashes = [];
allDomains.forEach(domain => {
    const lowerDomain = domain.toLowerCase();
    const hash = hashString(lowerDomain);
    const hexHash = formatHexHash(hash);
    
    console.log(`${lowerDomain.padEnd(25)} -> ${hexHash} (${hash})`);
    hashes.push(hash);
});

console.log();
console.log('='.repeat(60));
console.log('COPY THE FOLLOWING TO YOUR SOURCE FILES:');
console.log('='.repeat(60));
console.log();

console.log('1. For C++ (source/lib/domain_protection.cpp):');
console.log('-'.repeat(50));
console.log('static const unsigned long AUTHORIZED_HASHES[] = {');
allDomains.forEach((domain, index) => {
    const hash = hashString(domain.toLowerCase());
    const hexHash = formatHexHash(hash);
    const comment = `// Hash of ${domain.toLowerCase()}`;
    console.log(`    ${hexHash},  ${comment}`);
});
console.log('    0x00000000U   // Terminator');
console.log('};');
console.log();

console.log('2. For Node.js (website/node.js/apis/security/domain-validation.js):');
console.log('-'.repeat(70));
console.log('const AUTHORIZED_DOMAIN_HASHES = [');
allDomains.forEach((domain, index) => {
    const hash = hashString(domain.toLowerCase());
    const comment = `// ${domain.toLowerCase()}`;
    console.log(`    ${hash}, ${comment}`);
});
console.log('];');
console.log();

console.log('='.repeat(60));
console.log('SECURITY NOTES:');
console.log('='.repeat(60));
console.log('1. Change the XOR_KEY in both C++ and Node.js files to a unique value');
console.log('2. Test thoroughly with your actual domains before deployment');
console.log('3. The game will only work on the domains listed above');
console.log('4. Keep this generator script private - don\'t include it in public repos');
console.log('5. Consider additional obfuscation for production builds');
console.log();

// Generate a suggested new XOR key
const newXorKey = '0x' + Math.floor(Math.random() * 0xFFFFFFFF).toString(16).toUpperCase().padStart(8, '0');
console.log(`Suggested new XOR_KEY: ${newXorKey}`);
console.log('(Change this in domain_protection.cpp and domain-validation.js)');
console.log(); 