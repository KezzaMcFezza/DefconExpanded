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
const fs = require('fs');

const {
    pool,
    upload
} = require('../../constants');

const {
    authenticateToken,
    checkRole
}   = require('../../authentication')

router.post('/api/upload-demo', authenticateToken, upload.fields([
    { name: 'demoFile', maxCount: 1 },
    { name: 'jsonFile', maxCount: 1 }
]), checkRole(4), async (req, res) => {
    const clientIp = getClientIp(req);
    console.log(`Admin action initiated: Demo upload by ${req.user.username} from IP ${clientIp}`);

    if (!req.files || !req.files.demoFile || !req.files.jsonFile) {
        console.log(`Failed demo upload attempt by ${req.user.username} from IP ${clientIp}: Missing required files`);
        return res.status(400).json({ error: 'Both demo and JSON files are required' });
    }

    const demoFile = req.files.demoFile[0];
    const jsonFile = req.files.jsonFile[0];

    console.log(`Received files for upload by ${req.user.username} from IP ${clientIp}:`,
        JSON.stringify({ demo: demoFile.originalname, json: jsonFile.originalname }, null, 2));
    try {
        const [existingDemos] = await pool.query('SELECT * FROM demos WHERE name = ?', [demoFile.originalname]);
        if (existingDemos.length > 0) {
            console.log('Demo already exists:', demoFile.originalname);
            return res.status(400).json({ error: 'A demo with this name already exists' });
        }

        const jsonContent = await fs.promises.readFile(jsonFile.path, 'utf8');
        const logData = JSON.parse(jsonContent);

        await processDemoFile(demoFile.originalname, demoFile.size, logData, jsonFile.originalname);

        console.log(`Demo successfully uploaded and processed by ${req.user.username} from IP ${clientIp}: ${demoFile.originalname}`);
        res.json({ message: 'Demo uploaded and processed successfully' });
    } catch (error) {
        console.error(`Error processing uploaded demo by ${req.user.username} from IP ${clientIp}:`, error);
        res.status(500).json({ error: 'Unable to process uploaded demo', details: error.message });
    }
});

router.get('/api/demo/:demoId', authenticateToken, async (req, res) => {
    try {
        const [rows] = await pool.query('SELECT * FROM demos WHERE id = ?', [req.params.demoId]);
        if (rows.length === 0) {
            res.status(404).json({ error: 'Demo not found' });
        } else {
            res.json(rows[0]);
        }
    } catch (error) {
        console.error('Error fetching demo details:', error);
        res.status(500).json({ error: 'Unable to fetch demo details' });
    }
});

router.put('/api/demo/:demoId', authenticateToken, checkRole(5), async (req, res) => {
    const { demoId } = req.params;
    const { name, game_type, duration, players } = req.body;

    try {
        console.log('Starting database transaction');
        await pool.query('START TRANSACTION');

        const [oldData] = await pool.query('SELECT * FROM demos WHERE id = ?', [demoId]);
        console.log('Executing update query');
        const [updateResult] = await pool.query(
            'UPDATE demos SET name = ?, game_type = ?, duration = ?, players = ? WHERE id = ?',
            [name, game_type, duration, players, demoId]
        );

        console.log('Update result:', JSON.stringify(updateResult, null, 2));

        if (updateResult.affectedRows === 0) {
            console.log('No rows affected. Rolling back transaction.');
            await pool.query('ROLLBACK');
            return res.status(404).json({ error: 'Demo not found or no changes made' });
        }

        console.log('Committing transaction');
        await pool.query('COMMIT');
        console.log(`${req.user.username} updated demo:
        Demo ID: ${demoId}
        Old data: ${JSON.stringify(oldData[0], null, 2)}
        New data: ${JSON.stringify({ name, game_type, duration, players }, null, 2)}`);
        res.json({ message: 'Demo updated successfully' });
    } catch (error) {
        console.error('Error updating demo:', error.message);
        await pool.query('ROLLBACK');
        console.error('Error updating demo:', error);
        res.status(500).json({ error: 'An internal server error occurred' });
    }
});

router.get('/api/demo-profile-panel', async (req, res) => {
    const { playerName } = req.query;

    try {
        let query = 'SELECT * FROM demos';
        let params = [];
        let conditions = [];

        if (playerName) {
            conditions.push(`(player1_name LIKE ? OR player2_name LIKE ? OR player3_name LIKE ? OR player4_name LIKE ?
                         OR player5_name LIKE ? OR player6_name LIKE ? OR player7_name LIKE ? OR player8_name LIKE ?
                         OR player9_name LIKE ? OR player10_name LIKE ?)`);
            params = Array(10).fill(`%${playerName}%`);
        }

        if (conditions.length > 0) {
            query += ' WHERE ' + conditions.join(' AND ');
        }

        query += ' ORDER BY date DESC';

        const [demos] = await pool.query(query, params);

        const processedDemos = await Promise.all(demos.map(async (demo) => {
            let gameData = { players: [], spectators: [] };
            try {
                if (demo.players) {
                    const parsedData = JSON.parse(demo.players);
                    if (typeof parsedData === 'object') {
                        if (parsedData.players && Array.isArray(parsedData.players)) {
                            gameData.players = parsedData.players;
                        }
                        if (parsedData.spectators && Array.isArray(parsedData.spectators)) {
                            gameData.spectators = parsedData.spectators;
                        }
                    }
                }
            } catch (error) {
                console.error('Error parsing players data:', error);
            }

            const updatedDemo = {
                ...demo,
                players: JSON.stringify(gameData.players),
                spectators: JSON.stringify(gameData.spectators)
            };

            return updatedDemo;
        }));

        res.json({ demos: processedDemos });

    } catch (error) {
        console.error('Error fetching demos for profile/admin panel:', error);
        res.status(500).json({ error: 'Unable to fetch demos for profile/admin panel' });
    }
});

router.get('/api/all-demos', authenticateToken, checkRole(5), async (req, res) => {
    try {
        const [demos] = await pool.query('SELECT * FROM demos ORDER BY date DESC');
        res.json(demos);
    } catch (error) {
        console.error('Error fetching all demos:', error);
        res.status(500).json({ error: 'Unable to fetch all demos' });
    }
});

router.delete('/api/demo/:demoId', authenticateToken, checkRole(1), async (req, res) => {
    const clientIp = getClientIp(req);
    console.log(`Admin action initiated: Demo deletion by ${req.user.username} from IP ${clientIp}`);

    try {
        await pool.query('START TRANSACTION');

        const [demoData] = await pool.query('SELECT * FROM demos WHERE id = ?', [req.params.demoId]);
        if (demoData.length === 0) {
            await pool.query('ROLLBACK');
            return res.status(404).json({ error: 'Demo not found' });
        }

        await pool.query(
            'INSERT INTO deleted_demos (demo_name, deleted_by) VALUES (?, ?)',
            [demoData[0].name, req.user.id]
        );

        await pool.query('DELETE FROM demo_reports WHERE demo_id = ?', [req.params.demoId]);

        const [result] = await pool.query('DELETE FROM demos WHERE id = ?', [req.params.demoId]);

        await pool.query('COMMIT');

        console.log(`Demo successfully deleted by ${req.user.username} from IP ${clientIp}:`);
        console.log(JSON.stringify(demoData[0], null, 2));

        res.json({ message: 'Demo and associated data deleted successfully' });
    } catch (error) {
        await pool.query('ROLLBACK');
        console.error(`Error deleting demo by ${req.user.username} from IP ${clientIp}:`, error.message);
        res.status(500).json({ error: 'Unable to delete demo' });
    }
});

module.exports = router;