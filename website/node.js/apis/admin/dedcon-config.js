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

const express = require('express');
const router = express.Router();
const path = require('path');
const fs = require('fs');

const {
    configFiles,
    rootDir
} = require('../../constants');

const {
    authenticateToken,
} = require('../../authentication')

const permissions = require('../../permission-index');

const debug = require('../../debug-helpers');

const configFilePermissions = {
    '1v1config.txt': permissions.CONFIG_1V1_RANDOM,
    '1v1configdefault.txt': permissions.CONFIG_1V1_DEFAULT,
    '1v1configcursed.txt': permissions.CONFIG_1V1_CURSED,
    '1v1configraizer.txt': permissions.CONFIG_1V1_RAIZER,
    '1v1configbest.txt': permissions.CONFIG_1V1_BEST_SETUPS_1,
    '1v1configbest2.txt': permissions.CONFIG_1V1_BEST_SETUPS_2,
    '1v1configtest.txt': permissions.CONFIG_1V1_TEST,
    '1v1configvariable.txt': permissions.CONFIG_1V1_VARIABLE,
    '1v1configukbalanced.txt': permissions.CONFIG_1V1_UK_BALANCED,
    '2v2config.txt': permissions.CONFIG_2V2_DEFAULT,
    '2v2configukbalanced.txt': permissions.CONFIG_2V2_UK_BALANCED,
    '2v2tournament.txt': permissions.CONFIG_2V2_TOURNAMENT,
    '6playerffaconfig.txt': permissions.CONFIG_6PLAYER_FFA,
    '3v3ffaconfig.txt': permissions.CONFIG_3V3_FFA,
    '4v4config.txt': permissions.CONFIG_4V4_DEFAULT,
    '5v5config.txt': permissions.CONFIG_5V5_DEFAULT,
    '8playerdiplo.txt': permissions.CONFIG_8PLAYER_DIPLO,
    '10playerdiplo.txt': permissions.CONFIG_10PLAYER_DIPLO,
    '16playerconfig.txt': permissions.CONFIG_16PLAYER_DEFAULT,
    'ukmoddiplo.txt': permissions.CONFIG_UK_MOD_DIPLO,
    'muriconukmod.txt': permissions.CONFIG_MURICON_UK_MOD,
    'sonyhoovconfig.txt': permissions.CONFIG_SONY_HOOV,
    'noobfriendly.txt': permissions.CONFIG_NOOB_FRIENDLY,
    '1v1muriconrandom.txt': permissions.CONFIG_MURICON_RANDOM,
    '1v1muricon.txt': permissions.CONFIG_MURICON_DEFAULT,
    'hawhaw2v2config.txt': permissions.CONFIG_HAWHAW_2V2
};

function hasConfigPermission(userPermissions, filename) {
    const startTime = debug.enter('hasConfigPermission', arguments, 1);
    try {
        const requiredPermission = configFilePermissions[filename];
        if (!requiredPermission) {
            return false; 
        }
        return userPermissions.includes(requiredPermission) || userPermissions.includes(permissions.ADMIN_CONFIGURE_WEBSITE);
    } finally {
        debug.exit('hasConfigPermission', startTime);
    }
}

router.get('/api/config-files', authenticateToken, async (req, res) => {
    try {
        const configPath = rootDir;
        console.log(`Current working directory: ${process.cwd()}`);
        console.log(`Searching for config files in: ${configPath}`);

        let existingFiles = [];
        for (const filename of configFiles) {
            if (hasConfigPermission(req.user.permissions, filename)) {
                const filePath = path.join(configPath, filename);
                try {
                    await fs.promises.access(filePath, fs.constants.R_OK);
                    existingFiles.push(filename);
                    console.log(`Found file: ${filePath}`);
                } catch (err) {
                    console.log(`File not found: ${filePath}`);
                }
            }
        }
        res.json(existingFiles);
    } catch (error) {
        console.error('Error reading config files:', error);
        res.status(500).json({ error: 'Unable to read config files' });
    }
});

router.get('/api/config-files/:filename', authenticateToken, async (req, res) => {
    try {
        if (!configFiles.includes(req.params.filename)) {
            return res.status(403).json({ error: 'Invalid file' });
        }
        
        if (!hasConfigPermission(req.user.permissions, req.params.filename)) {
            return res.status(403).json({ error: 'Insufficient permissions for this config file' });
        }
        
        const filePath = path.join(rootDir, req.params.filename);
        const content = await fs.promises.readFile(filePath, 'utf8');
        res.json({ content });
    } catch (error) {
        console.error('Error reading file:', error);
        res.status(500).json({ error: 'Unable to read file' });
    }
});

router.put('/api/config-files/:filename', authenticateToken, async (req, res) => {
    try {
        if (!configFiles.includes(req.params.filename)) {
            return res.status(403).json({ error: 'Invalid file' });
        }
        
        if (!hasConfigPermission(req.user.permissions, req.params.filename)) {
            return res.status(403).json({ error: 'Insufficient permissions for this config file' });
        }
        
        const filePath = path.join(rootDir, req.params.filename);
        await fs.promises.writeFile(filePath, req.body.content);
        console.log(`${req.user.username} updated config file: ${req.params.filename}`);
        res.json({ message: 'File updated successfully' });
    } catch (error) {
        console.error('Error writing file:', error);
        res.status(500).json({ error: 'Unable to write file' });
    }
});

module.exports = router;