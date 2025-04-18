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
    pool,
    resourcesDir,
    upload
} = require('../../constants');

const {
    authenticateToken,
    checkRole
}   = require('../../authentication')

const {
    getClientIp
}   = require('../../shared-functions')

router.post('/api/upload-resource', authenticateToken, upload.single('resourceFile'), checkRole(2), async (req, res) => {
    const clientIp = getClientIp(req);

    console.log(`Admin action initiated: Resource upload by ${req.user.username} from IP ${clientIp}`);

    if (!req.file) {
        console.log(`Failed resource upload attempt by ${req.user.username} from IP ${clientIp}: No file uploaded`);
        return res.status(400).json({ error: 'No file uploaded' });
    }

    try {
        const originalName = req.file.originalname;
        const filePath = path.join(resourcesDir, originalName);
        const { version, releaseDate, platform, playerCount } = req.body;

        if (!version || !platform || !playerCount) {
            console.log(`Failed resource upload attempt by ${req.user.username} from IP ${clientIp}: Missing required fields`);
            return res.status(400).json({ error: 'Version, platform, and player count are required' });
        }

        console.log(`Processing resource upload by ${req.user.username} from IP ${clientIp}:`,
            JSON.stringify({ name: originalName, version, releaseDate, platform, playerCount }, null, 2));


        fs.renameSync(req.file.path, filePath);

        const stats = fs.statSync(filePath);

        const uploadDate = releaseDate ? new Date(releaseDate) : new Date();

        const [result] = await pool.query(
            'INSERT INTO resources (name, size, date, version, platform, player_count) VALUES (?, ?, ?, ?, ?, ?)',
            [originalName, stats.size, uploadDate, version, platform, playerCount]
        );

        console.log(`Resource successfully uploaded by ${req.user.username} from IP ${clientIp}: ${originalName} (Version: ${version}, Platform: ${platform}, Player Count: ${playerCount})`);
        res.json({ message: 'Resource uploaded successfully', resourceName: originalName, version: version, platform: platform, playerCount: playerCount });
    } catch (error) {
        console.error(`Error uploading resource by ${req.user.username} from IP ${clientIp}:`, error.message);

        if (req.file && req.file.path) {
            try {
                fs.unlinkSync(req.file.path);
            } catch (unlinkError) {
                console.error('Error deleting uploaded file:', unlinkError.message);
            }
        }

        res.status(500).json({ error: 'Unable to upload resource', details: error.message });
    }
});


router.delete('/api/resource/:resourceId', authenticateToken, checkRole(1), async (req, res) => {
    const clientIp = getClientIp(req);
    console.log(`Admin action initiated: Resource deletion by ${req.user.username} from IP ${clientIp}`);

    try {
        const [resourceData] = await pool.query('SELECT name, version FROM resources WHERE id = ?', [req.params.resourceId]);
        const [result] = await pool.query('DELETE FROM resources WHERE id = ?', [req.params.resourceId]);
        if (result.affectedRows === 0) {

            console.log(`Failed resource deletion attempt by ${req.user.username} from IP ${clientIp}: Resource not found (ID: ${req.params.resourceId})`);
            res.status(404).json({ error: 'Resource not found' });
        } else {
            console.log(`Resource successfully deleted by ${req.user.username} from IP ${clientIp}: ${resourceData[0].name} (Version: ${resourceData[0].version}, ID: ${req.params.resourceId})`);
            res.json({ message: 'Resource deleted successfully' });
        }
    } catch (error) {
        console.error(`Error deleting resource by ${req.user.username} from IP ${clientIp}:`, error);
        res.status(500).json({ error: 'Unable to delete resource' });
    }
});


router.put('/api/resource/:resourceId', authenticateToken, checkRole(2), async (req, res) => {
    const clientIp = getClientIp(req);
    const { resourceId } = req.params;
    const { name, version, date, platform, playerCount } = req.body;


    console.log(`Admin action initiated: Resource edit by ${req.user.username} from IP ${clientIp}`);
    console.log(`Editing resource ID ${resourceId}:`, JSON.stringify({ name, version, date, platform, playerCount }, null, 2));

    try {
        const [oldData] = await pool.query('SELECT * FROM resources WHERE id = ?', [resourceId]);
        const [result] = await pool.query(
            'UPDATE resources SET name = ?, version = ?, date = ?, platform = ?, player_count = ? WHERE id = ?',
            [name, version, new Date(date), platform, playerCount, resourceId]
        );

        if (result.affectedRows === 0) {
            console.log(`Failed resource edit attempt by ${req.user.username} from IP ${clientIp}: Resource not found (ID: ${resourceId})`);
            res.status(404).json({ error: 'Resource not found' });
        } else {
            console.log(`Resource successfully edited by ${req.user.username} from IP ${clientIp}:`);
            console.log(`Old data:`, JSON.stringify(oldData[0], null, 2));
            console.log(`New data:`, JSON.stringify({ name, version, date, platform, playerCount }, null, 2));
            res.json({ message: 'Resource updated successfully' });
        }
    } catch (error) {
        console.error(`Error updating resource by ${req.user.username} from IP ${clientIp}:`, error.message);
        res.status(500).json({ error: 'Unable to update resource' });
    }
});

router.get('/api/resource/:resourceId', authenticateToken, async (req, res) => {
    try {
        const [rows] = await pool.query('SELECT * FROM resources WHERE id = ?', [req.params.resourceId]);
        if (rows.length === 0) {
            res.status(404).json({ error: 'Resource not found' });
        } else {
            res.json(rows[0]);
        }
    } catch (error) {
        console.error('Error fetching resource details:', error);
        res.status(500).json({ error: 'Unable to fetch resource details' });
    }
});

module.exports = router;