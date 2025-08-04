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
//Last Edited 04-08-2025

const express = require('express');
const router = express.Router();
const path = require('path');
const fs = require('fs');

const {
    pool,
    replayBuildsDir,
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

router.get('/api/admin/replay-builds', authenticateToken, checkPermission(permissions.REPLAY_VIEW), async (req, res) => {
    try {
        const [builds] = await pool.query('SELECT * FROM replay_builds ORDER BY release_date DESC');
        res.json(builds);
    } catch (error) {
        console.error('Error fetching replay builds for admin:', error);
        res.status(500).json({ error: 'Unable to fetch replay builds' });
    }
});

router.post('/api/upload-replay-build', authenticateToken, upload.single('replayBuildsFile'), checkPermission(permissions.REPLAY_ADD), async (req, res) => {
    const clientIp = getClientIp(req);
    console.log(`Replay build upload by ${req.user.username} from IP ${clientIp}`);

    if (!req.file) {
        console.log(`Failed replay build upload attempt by ${req.user.username} from IP ${clientIp}: No file uploaded`);
        return res.status(400).json({ error: 'No file uploaded' });
    }

    try {
        const { version, releaseDate, platform } = req.body;
        const originalName = req.file.originalname;

        const filePath = path.join(replayBuildsDir, originalName);

        if (!version || !platform) {
            console.log(`Failed replay build upload attempt by ${req.user.username} from IP ${clientIp}: Missing required fields`);
            return res.status(400).json({ error: 'Version and platform are required' });
        }

        console.log(`Processing replay build upload by ${req.user.username} from IP ${clientIp}:`,
            JSON.stringify({ name: originalName, version, releaseDate, platform }, null, 2));

        fs.renameSync(req.file.path, filePath);
        const stats = fs.statSync(filePath);
        const uploadDate = releaseDate ? new Date(releaseDate) : new Date();

        const [result] = await pool.query(
            'INSERT INTO replay_builds (name, size, release_date, version, platform, file_path) VALUES (?, ?, ?, ?, ?, ?)',
            [originalName, stats.size, uploadDate, version, platform, filePath]
        );

        console.log(`Replay build successfully uploaded by ${req.user.username} from IP ${clientIp}`);
        res.json({
            message: 'Replay build uploaded successfully',
            buildName: originalName,
            version: version,
            platform: platform
        });
    } catch (error) {
        console.error(`Error uploading replay build by ${req.user.username} from IP ${clientIp}:`, error.message);

        if (req.file && req.file.path) {
            try {
                fs.unlinkSync(req.file.path);
            } catch (unlinkError) {
                console.error('Error deleting uploaded file:', unlinkError.message);
            }
        }

        res.status(500).json({ error: 'Unable to upload replay build', details: error.message });
    }
});

router.delete('/api/replay-build/:buildId', authenticateToken, checkPermission(permissions.REPLAY_DELETE), async (req, res) => {
    const clientIp = getClientIp(req);
    console.log(`Replay build deletion by ${req.user.username} from IP ${clientIp}`);

    try {
        const [buildData] = await pool.query('SELECT * FROM replay_builds WHERE id = ?', [req.params.buildId]);
        const [result] = await pool.query('DELETE FROM replay_builds WHERE id = ?', [req.params.buildId]);

        if (result.affectedRows === 0) {
            console.log(`Failed replay build deletion attempt by ${req.user.username} from IP ${clientIp}: Build not found (ID: ${req.params.buildId})`);
            res.status(404).json({ error: 'Replay build not found' });
        } else {
            console.log(`Replay build successfully deleted by ${req.user.username} from IP ${clientIp}:`, JSON.stringify(buildData[0], null, 2));
            res.json({ message: 'Replay build deleted successfully' });
        }
    } catch (error) {
        console.error(`Error deleting replay build by ${req.user.username} from IP ${clientIp}:`, error);
        res.status(500).json({ error: 'Unable to delete replay build' });
    }
});

router.get('/api/replay-build/:buildId', authenticateToken, checkPermission(permissions.REPLAY_VIEW), async (req, res) => {
    try {
        const [rows] = await pool.query('SELECT * FROM replay_builds WHERE id = ?', [req.params.buildId]);
        if (rows.length === 0) {
            res.status(404).json({ error: 'Replay build not found' });
        } else {
            res.json(rows[0]);
        }
    } catch (error) {
        console.error('Error fetching replay build details:', error);
        res.status(500).json({ error: 'Unable to fetch replay build details' });
    }
});

router.put('/api/replay-build/:buildId', authenticateToken, checkPermission(permissions.REPLAY_EDIT), async (req, res) => {
    const clientIp = getClientIp(req);
    const { buildId } = req.params;
    const { name, version, release_date, platform } = req.body;

    console.log(`Replay build edit by ${req.user.username} from IP ${clientIp}`);
    console.log(`Editing replay build ID ${buildId}:`, JSON.stringify({ name, version, release_date, platform }, null, 2));

    try {
        const [oldData] = await pool.query('SELECT * FROM replay_builds WHERE id = ?', [buildId]);
        const [result] = await pool.query(
            'UPDATE replay_builds SET name = ?, version = ?, release_date = ?, platform = ? WHERE id = ?',
            [name, version, new Date(release_date), platform, buildId]
        );

        if (result.affectedRows === 0) {
            console.log(`Failed replay build edit attempt by ${req.user.username} from IP ${clientIp}: Build not found (ID: ${buildId})`);
            res.status(404).json({ error: 'Replay build not found' });
        } else {
            console.log(`Replay build successfully edited by ${req.user.username} from IP ${clientIp}:`);
            console.log(`Old data:`, JSON.stringify(oldData[0], null, 2));
            console.log(`New data:`, JSON.stringify({ name, version, release_date, platform }, null, 2));
            res.json({ message: 'Replay build updated successfully' });
        }
    } catch (error) {
        console.error(`Error updating replay build by ${req.user.username} from IP ${clientIp}:`, error.message);
        res.status(500).json({ error: 'Unable to update replay build' });
    }
});

module.exports = router;