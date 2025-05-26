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
    pool,
    dedconBuildsDir,
    upload
} = require('../../constants');

const {
    authenticateToken,
    checkPermission
} = require('../../authentication')

const permissions = require('../../permission-index');

const {
    getClientIp
} = require('../../shared-functions');

const debug = require('../../debug-helpers');

router.get('/api/admin/dedcon-builds', authenticateToken, checkPermission(permissions.DEDCON_VIEW), async (req, res) => {
    try {
        const [builds] = await pool.query('SELECT * FROM dedcon_builds ORDER BY release_date DESC');
        res.json(builds);
    } catch (error) {
        console.error('Error fetching builds for admin:', error);
        res.status(500).json({ error: 'Unable to fetch builds' });
    }
});

router.post('/api/upload-dedcon-build', authenticateToken, upload.single('dedconBuildsFile'), checkPermission(permissions.DEDCON_ADD), async (req, res) => {
    const clientIp = getClientIp(req);
    console.log(`Build upload by ${req.user.username} from IP ${clientIp}`);

    if (!req.file) {
        console.log(`Failed build upload attempt by ${req.user.username} from IP ${clientIp}: No file uploaded`);
        return res.status(400).json({ error: 'No file uploaded' });
    }

    try {
        const { version, releaseDate, platform, playerCount, beta } = req.body;
        const originalName = req.file.originalname;

        const filePath = path.join(dedconBuildsDir, originalName);

        if (!version || !platform || !playerCount) {
            console.log(`Failed build upload attempt by ${req.user.username} from IP ${clientIp}: Missing required fields`);
            return res.status(400).json({ error: 'Version, platform, and player count are required' });
        }

        console.log(`Processing build upload by ${req.user.username} from IP ${clientIp}:`,
            JSON.stringify({ name: originalName, version, releaseDate, platform, playerCount, beta }, null, 2));


        fs.renameSync(req.file.path, filePath);
        const stats = fs.statSync(filePath);
        const uploadDate = releaseDate ? new Date(releaseDate) : new Date();
        const isBeta = beta ? 1 : 0;

        const [result] = await pool.query(
            'INSERT INTO dedcon_builds (name, size, release_date, version, platform, player_count, file_path, beta) VALUES (?, ?, ?, ?, ?, ?, ?, ?)',
            [originalName, stats.size, uploadDate, version, platform, playerCount, filePath, isBeta]
        );

        console.log(`Build successfully uploaded by ${req.user.username} from IP ${clientIp}`);
        res.json({
            message: 'Build uploaded successfully',
            buildName: originalName,
            version: version,
            platform: platform,
            playerCount: playerCount,
            beta: isBeta
        });
    } catch (error) {
        console.error(`Error uploading build by ${req.user.username} from IP ${clientIp}:`, error.message);

        if (req.file && req.file.path) {
            try {
                fs.unlinkSync(req.file.path);
            } catch (unlinkError) {
                console.error('Error deleting uploaded file:', unlinkError.message);
            }
        }

        res.status(500).json({ error: 'Unable to upload build', details: error.message });
    }
});

router.delete('/api/dedcon-build/:buildId', authenticateToken, checkPermission(permissions.DEDCON_DELETE), async (req, res) => {
    const clientIp = getClientIp(req);
    console.log(`Build deletion by ${req.user.username} from IP ${clientIp}`);

    try {
        const [buildData] = await pool.query('SELECT * FROM dedcon_builds WHERE id = ?', [req.params.buildId]);
        const [result] = await pool.query('DELETE FROM dedcon_builds WHERE id = ?', [req.params.buildId]);

        if (result.affectedRows === 0) {
            console.log(`Failed build deletion attempt by ${req.user.username} from IP ${clientIp}: Build not found (ID: ${req.params.buildId})`);
            res.status(404).json({ error: 'Build not found' });
        } else {
            console.log(`Build successfully deleted by ${req.user.username} from IP ${clientIp}:`, JSON.stringify(buildData[0], null, 2));
            res.json({ message: 'Build deleted successfully' });
        }
    } catch (error) {
        console.error(`Error deleting build by ${req.user.username} from IP ${clientIp}:`, error);
        res.status(500).json({ error: 'Unable to delete build' });
    }
});

router.get('/api/dedcon-build/:buildId', authenticateToken, checkPermission(permissions.DEDCON_VIEW), async (req, res) => {
    try {
        const [rows] = await pool.query('SELECT * FROM dedcon_builds WHERE id = ?', [req.params.buildId]);
        if (rows.length === 0) {
            res.status(404).json({ error: 'Build not found' });
        } else {
            res.json(rows[0]);
        }
    } catch (error) {
        console.error('Error fetching build details:', error);
        res.status(500).json({ error: 'Unable to fetch build details' });
    }
});

router.put('/api/dedcon-build/:buildId', authenticateToken, checkPermission(permissions.DEDCON_EDIT), async (req, res) => {
    const clientIp = getClientIp(req);
    const { buildId } = req.params;
    const { name, version, release_date, platform, player_count, beta } = req.body;
    const isBeta = beta ? 1 : 0;

    console.log(`Build edit by ${req.user.username} from IP ${clientIp}`);
    console.log(`Editing build ID ${buildId}:`, JSON.stringify({ name, version, release_date, platform, player_count, beta }, null, 2));

    try {
        const [oldData] = await pool.query('SELECT * FROM dedcon_builds WHERE id = ?', [buildId]);
        const [result] = await pool.query(
            'UPDATE dedcon_builds SET name = ?, version = ?, release_date = ?, platform = ?, player_count = ?, beta = ? WHERE id = ?',
            [name, version, new Date(release_date), platform, player_count, isBeta, buildId]
        );

        if (result.affectedRows === 0) {
            console.log(`Failed build edit attempt by ${req.user.username} from IP ${clientIp}: Build not found (ID: ${buildId})`);
            res.status(404).json({ error: 'Build not found' });
        } else {
            console.log(`Build successfully edited by ${req.user.username} from IP ${clientIp}:`);
            console.log(`Old data:`, JSON.stringify(oldData[0], null, 2));
            console.log(`New data:`, JSON.stringify({ name, version, release_date, platform, player_count, beta: isBeta }, null, 2));
            res.json({ message: 'Build updated successfully' });
        }
    } catch (error) {
        console.error(`Error updating build by ${req.user.username} from IP ${clientIp}:`, error.message);
        res.status(500).json({ error: 'Unable to update build' });
    }
});

module.exports = router;