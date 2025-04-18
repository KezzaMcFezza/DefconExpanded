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

const {
    pool,
    upload,
    publicDir
} = require('../../constants');

const {
    checkRole
}   = require('../../authentication')

const {
    getClientIp
}   = require('../../shared-functions')

router.get('/api/mods/:id', async (req, res) => {
    try {
        const [rows] = await pool.query('SELECT * FROM modlist WHERE id = ?', [req.params.id]);
        if (rows.length === 0) {
            res.status(404).json({ error: 'Mod not found' });
        } else {
            res.json(rows[0]);
        }
    } catch (error) {
        console.error('Error fetching mod details:', error);
        res.status(500).json({ error: 'Unable to fetch mod details' });
    }
});


router.post('/api/mods', upload.fields([
    { name: 'modFile', maxCount: 1 },
    { name: 'previewImage', maxCount: 1 }
]), checkRole(5), async (req, res) => {
    try {
        const { name, type, creator, releaseDate, description, compatibility, version } = req.body;
        const modFile = req.files['modFile'] ? req.files['modFile'][0] : null;
        const previewImage = req.files['previewImage'] ? req.files['previewImage'][0] : null;
        const modFilePath = modFile ? path.join(publicDir, 'modlist', modFile.originalname).replace(/\\/g, '/') : null;
        const previewImagePath = previewImage ? path.join('public', 'modpreviews', previewImage.originalname).replace(/\\/g, '/') : null;
        const fileSize = modFile ? modFile.size : 0;

        
        const [result] = await pool.query(
            'INSERT INTO modlist (name, type, creator, release_date, description, file_path, preview_image_path, compatibility, version, size) VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?)',
            [name, type, creator, releaseDate, description, modFilePath, previewImagePath, compatibility, version, fileSize]
        );

        res.json({ id: result.insertId, message: 'Mod added successfully' });
    } catch (error) {
        console.error('Error adding mod:', error);
        res.status(500).json({ error: 'Unable to add mod', details: error.message });
    }
});


router.put('/api/mods/:id', upload.fields([
    { name: 'modFile', maxCount: 1 },
    { name: 'previewImage', maxCount: 1 }
]), checkRole(5), async (req, res) => {
    const { id } = req.params;
    const { name, type, creator, releaseDate, description, compatibility, version } = req.body;
    const clientIp = getClientIp(req);

    console.log(`Admin action initiated: Mod update by ${req.user.username} from IP ${clientIp}`);

    try {
        
        const [oldData] = await pool.query('SELECT * FROM modlist WHERE id = ?', [id]);
        if (oldData.length === 0) {
            console.log(`Failed mod update attempt by ${req.user.username} from IP ${clientIp}: Mod not found (ID: ${id})`);
            return res.status(404).json({ error: 'Mod not found' });
        }

        let query = 'UPDATE modlist SET name = ?, type = ?, creator = ?, release_date = ?, description = ?, compatibility = ?, version = ?';
        let params = [name, type, creator, releaseDate, description, compatibility, version];
        let newFilePath = oldData[0].file_path;
        let newFileSize = oldData[0].size;
        let newPreviewPath = oldData[0].preview_image_path;

        if (req.files['modFile']) {
            const modFile = req.files['modFile'][0];
            newFilePath = path.join('public', 'modlist', modFile.originalname).replace(/\\/g, '/');
            newFileSize = modFile.size;
            query += ', file_path = ?, size = ?';
            params.push(newFilePath, newFileSize);
        }

        if (req.files['previewImage']) {
            const previewImage = req.files['previewImage'][0];
            newPreviewPath = path.join('public', 'modpreviews', previewImage.originalname).replace(/\\/g, '/');
            query += ', preview_image_path = ?';
            params.push(newPreviewPath);
        }

        query += ' WHERE id = ?';
        params.push(id);

        const [result] = await pool.query(query, params);

        if (result.affectedRows === 0) {
            console.log(`Failed mod update attempt by ${req.user.username} from IP ${clientIp}: No changes made (ID: ${id})`);
            return res.status(404).json({ error: 'Mod not found or no changes made' });
        }

        
        console.log(`Mod successfully updated by ${req.user.username} from IP ${clientIp}:`);
        console.log(`Mod ID: ${id}`);
        console.log('Old data:', JSON.stringify(oldData[0], null, 2));
        console.log('New data:', JSON.stringify({
            name, type, creator, release_date: releaseDate, description, compatibility, version,
            file_path: newFilePath,
            size: newFileSize,
            preview_image_path: newPreviewPath
        }, null, 2));

        res.json({ message: 'Mod updated successfully' });
    } catch (error) {
        console.error(`Error updating mod by ${req.user.username} from IP ${clientIp}:`, error);
        res.status(500).json({ error: 'Unable to update mod', details: error.message });
    }
});


router.delete('/api/mods/:id', checkRole(1), async (req, res) => {
    const { id } = req.params;

    try {
        await pool.query('DELETE FROM modlist WHERE id = ?', [id]);
        res.json({ message: 'Mod deleted successfully' });
    } catch (error) {
        console.error('Error deleting mod:', error);
        res.status(500).json({ error: 'Unable to delete mod' });
    }
});

module.exports = router;