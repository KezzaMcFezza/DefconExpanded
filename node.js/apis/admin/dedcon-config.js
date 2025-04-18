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
//Last Edited 18-04-2025

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
    checkRole
}   = require('../../authentication')

router.get('/api/config-files', authenticateToken, checkRole(2), async (req, res) => {
    try {
        const configPath = rootDir;
        console.log(`Current working directory: ${process.cwd()}`);
        console.log(`Searching for config files in: ${configPath}`);

        let existingFiles = [];
        for (const filename of configFiles) {
            const filePath = path.join(configPath, filename);
            try {
                await fs.promises.access(filePath, fs.constants.R_OK);
                existingFiles.push(filename);
                console.log(`Found file: ${filePath}`);
            } catch (err) {
                console.log(`File not found: ${filePath}`);
            }
        }
        res.json(existingFiles);
    } catch (error) {
        console.error('Error reading config files:', error);
        res.status(500).json({ error: 'Unable to read config files' });
    }
});

router.get('/api/config-files/:filename', authenticateToken, checkRole(2), async (req, res) => {
    try {
        if (!configFiles.includes(req.params.filename)) {
            return res.status(403).json({ error: 'Invalid file' });
        }
        const filePath = path.join(rootDir, req.params.filename);
        const content = await fs.promises.readFile(filePath, 'utf8');
        res.json({ content });
    } catch (error) {
        console.error('Error reading file:', error);
        res.status(500).json({ error: 'Unable to read file' });
    }
});

router.put('/api/config-files/:filename', authenticateToken, checkRole(2), async (req, res) => {
    try {
        if (!configFiles.includes(req.params.filename)) {
            return res.status(403).json({ error: 'Invalid file' });
        }
        const filePath = path.join(rootDir, req.params.filename);
        await fs.promises.writeFile(filePath, req.body.content);
        res.json({ message: 'File updated successfully' });
    } catch (error) {
        console.error('Error writing file:', error);
        res.status(500).json({ error: 'Unable to write file' });
    }
});

module.exports = router;