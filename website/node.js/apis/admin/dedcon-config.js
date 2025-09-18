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
    'DefconExpanded Test Server.txt': permissions.CONFIG_1V1_TEST,
    'DefconExpanded 1v1 Totally Random.txt': permissions.CONFIG_1V1_RANDOM,
    'DefconExpanded 1v1 Default.txt': permissions.CONFIG_1V1_DEFAULT,
    'DefconExpanded 1v1 Best Setups Only.txt': permissions.CONFIG_1V1_BEST_SETUPS_1,
    'DefconExpanded 1v1 Best Setups Only Second Server.txt': permissions.CONFIG_1V1_BEST_SETUPS_2,
    'DefconExpanded 1v1 Cursed Setups Only.txt': permissions.CONFIG_1V1_CURSED,
    'DefconExpanded 1v1 Lots of Units.txt': permissions.CONFIG_1V1_VARIABLE,
    'DefconExpanded 1v1 UK and Ireland.txt': permissions.CONFIG_1V1_UK_BALANCED,
    'DefconExpanded 1v1 Totally Random - Second Server.txt': permissions.CONFIG_1V1_RANDOM,
    'MURICON 1v1 Default 2.8.15.txt': permissions.CONFIG_MURICON_DEFAULT,
    'DefconExpanded 2v2 Totally Random.txt': permissions.CONFIG_2V2_DEFAULT,
    'DefconExpanded 2v2 UK and Ireland.txt': permissions.CONFIG_2V2_UK_BALANCED,
    'DefconExpanded 2v2 Tournament.txt': permissions.CONFIG_2V2_TOURNAMENT,
    'DefconExpanded 2v2 Max Cities - Pop.txt': permissions.CONFIG_2V2_MAX_CITIES,
    'MURICON 1v1 Totally Random 2.8.15.txt': permissions.CONFIG_MURICON_RANDOM,
    '509 CG 1v1 Totally Random 2.8.15.txt': permissions.CONFIG_509CG_1V1,
    '509 CG 2v2 Totally Random 2.8.15.txt': permissions.CONFIG_HAWHAW_2V2,
    'Raizer\'s Russia vs USA Totally Random.txt': permissions.CONFIG_1V1_RAIZER,
    'Sony and Hoov\'s Hideout.txt': permissions.CONFIG_SONY_HOOV,
    'New Players Welcome Come and Play.txt': permissions.CONFIG_NOOB_FRIENDLY,
    'DefconExpanded 3v3 Totally Random.txt': permissions.CONFIG_3V3_FFA,
    'DefconExpanded 3v3 Totally Random Second Server.txt': permissions.CONFIG_3V3_FFA,
    'DefconExpanded 3v3 Totally Random - Second Server.txt': permissions.CONFIG_3V3_FFA,
    'MURICON UK Mod.txt': permissions.CONFIG_MURICON_UK_MOD,
    'DefconExpanded Free For All Random Cities.txt': permissions.CONFIG_6PLAYER_FFA,
    'DefconExpanded Training.txt': permissions.CONFIG_TRAINING,
    'DefconExpanded Diplomacy UK and Ireland.txt': permissions.CONFIG_UK_MOD_DIPLO,
    'DefconExpanded 4v4 Totally Random.txt': permissions.CONFIG_4V4_DEFAULT,
    'DefconExpanded 5v5 Totally Random.txt': permissions.CONFIG_5V5_DEFAULT,
    'DefconExpanded 8 Player Diplomacy.txt': permissions.CONFIG_8PLAYER_DIPLO,
    'DefconExpanded 10 Player Diplomacy.txt': permissions.CONFIG_10PLAYER_DIPLO,
    'DefconExpanded 16 Player Test Server.txt': permissions.CONFIG_16PLAYER_DEFAULT
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
        const configPath = path.join(rootDir, 'dedcon_configs');
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
        
        const filePath = path.join(rootDir, 'dedcon_configs', req.params.filename);
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
        
        const filePath = path.join(rootDir, 'dedcon_configs', req.params.filename);
        await fs.promises.writeFile(filePath, req.body.content);
        console.log(`${req.user.username} updated config file: ${req.params.filename}`);
        res.json({ message: 'File updated successfully' });
    } catch (error) {
        console.error('Error writing file:', error);
        res.status(500).json({ error: 'Unable to write file' });
    }
});

module.exports = router;