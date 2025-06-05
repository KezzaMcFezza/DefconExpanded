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
//Last Edited 05-06-2025

const express = require('express');
const router = express.Router();

const {
    pool,
    transporter
} = require('../../constants');

const debug = require('../../debug-helpers');

const { authenticateToken } = require('../../authentication');

router.get('/api/smurf-check', async (req, res) => {
  const startTime = debug.enter('smurfCheck', [req.query.playerName], 1);
  const { playerName } = req.query;

  debug.level2('Smurf check request:', { playerName });

  if (!playerName || playerName.trim().length === 0) {
    debug.level1('Smurf check missing player name');
    debug.exit('smurfCheck', startTime, 'missing_player_name', 1);
    return res.status(400).json({ error: 'Player name is required' });
  }

  try {
    
    const [demos] = await pool.query('SELECT id, name, players, game_type, date, duration, download_count FROM demos ORDER BY date DESC');
    const playerMap = new Map();
    const keyIdMap = new Map();
    const ipMap = new Map();

    
    demos.forEach(demo => {
      try {
        
        if (!demo.players || demo.players === 'null' || demo.players === '') {
          return;
        }
        
        const parsedData = JSON.parse(demo.players);
        if (!parsedData) {
          return;
        }
        
        const players = Array.isArray(parsedData) ? parsedData : (parsedData.players || []);

        players.forEach(player => {
          if (!player.name) return;

          if (!playerMap.has(player.name)) {
            playerMap.set(player.name, {
              gamesPlayed: 0,
              alternateNames: new Set(),
              rating: 50,
              infractionCount: 0
            });
          }

          const playerData = playerMap.get(player.name);
          playerData.gamesPlayed++;

          
          if (player.ip && player.ip.trim() !== '') {
            if (!ipMap.has(player.ip)) {
              ipMap.set(player.ip, new Set());
            }
            ipMap.get(player.ip).add(player.name);
          }

          if (player.key_id && player.key_id.trim() !== '' && player.key_id !== 'DEMO') {
            if (!keyIdMap.has(player.key_id)) {
              keyIdMap.set(player.key_id, new Set());
            }
            keyIdMap.get(player.key_id).add(player.name);
          }
        });
      } catch (error) {
        console.error('Error processing demo:', error);
      }
    });

    playerMap.forEach((data, name) => {
      const alternateNamesSet = new Set(); 
      const infractionDemos = new Map(); 
  
      keyIdMap.forEach((names, keyId) => {
        if (names.has(name) && names.size > 1) {
          names.forEach(relatedName => {
            if (relatedName !== name) {
              alternateNamesSet.add(relatedName);
  
              demos.forEach(demo => {
                try {
                  
                  if (!demo.players || demo.players === 'null' || demo.players === '') {
                    return;
                  }
                  
                  const parsedData = JSON.parse(demo.players);
                  if (!parsedData) {
                    return;
                  }
                  
                  const players = Array.isArray(parsedData) ? parsedData : (parsedData.players || []);
                  const hasRelatedPlayerWithKeyId = players.some(p => p.name === relatedName && p.key_id === keyId);
                  
                  if (hasRelatedPlayerWithKeyId) {
                    
                    if (!infractionDemos.has(demo.id)) {
                      infractionDemos.set(demo.id, {
                        id: demo.id,
                        name: demo.name,
                        game_type: demo.game_type,
                        date: demo.date,
                        duration: demo.duration,
                        players: demo.players,
                        spectators: demo.spectators,
                        download_count: demo.download_count,
                        reason: `Alternate name detected: ${relatedName}`,
                        detectionType: 'authentication'
                      });
                    }
                  }
                } catch (error) {
                  console.error('Error processing demo for keyID infraction:', error);
                }
              });
            }
          });
        }
      });
      
      ipMap.forEach((names, ip) => {
        if (names.has(name) && names.size > 1) {
          names.forEach(relatedName => {
            if (relatedName !== name) {
              alternateNamesSet.add(relatedName);
              
              demos.forEach(demo => {
                try {
                  
                  if (!demo.players || demo.players === 'null' || demo.players === '') {
                    return;
                  }
                  
                  const parsedData = JSON.parse(demo.players);
                  if (!parsedData) {
                    return;
                  }
                  
                  const players = Array.isArray(parsedData) ? parsedData : (parsedData.players || []);
                  const hasRelatedPlayerWithIp = players.some(p => p.name === relatedName && p.ip === ip);
                  
                  if (hasRelatedPlayerWithIp) {
                    
                    if (!infractionDemos.has(demo.id)) {
                      infractionDemos.set(demo.id, {
                        id: demo.id,
                        name: demo.name,
                        game_type: demo.game_type,
                        date: demo.date,
                        duration: demo.duration,
                        players: demo.players,
                        spectators: demo.spectators,
                        download_count: demo.download_count,
                        reason: `Alternate name detected: ${relatedName}`,
                        detectionType: 'network'
                      });
                    }
                  }
                } catch (error) {
                  console.error('Error processing demo for IP infraction:', error);
                }
              });
            }
          });
        }
      });

      const alternateNamesMap = new Map();
      alternateNamesSet.forEach(alternateName => {
        let totalOccurrences = 0;
        
        demos.forEach(demo => {
          try {
            
            if (!demo.players || demo.players === 'null' || demo.players === '') {
              return;
            }
            
            const parsedData = JSON.parse(demo.players);
            if (!parsedData) {
              return;
            }
            
            const players = Array.isArray(parsedData) ? parsedData : (parsedData.players || []);
            const appearsInDemo = players.some(p => p.name === alternateName);
            if (appearsInDemo) {
              totalOccurrences++;
            }
          } catch (error) {
            console.error('Error counting occurrences for alternate name:', error);
          }
        });
        
        alternateNamesMap.set(alternateName, totalOccurrences);
      });

      data.alternateNames = alternateNamesMap;
      data.infractionCount = alternateNamesMap.size;
      data.rating = Math.max(0, 50 - (alternateNamesMap.size * 7));
      
      const sortedInfractionDemos = Array.from(infractionDemos.values())
        .sort((a, b) => new Date(b.date) - new Date(a.date))
        .slice(0, 100);
      
      data.infractionDemos = sortedInfractionDemos;
    });

    const searchTerm = playerName.toLowerCase();
    const matches = [];

    playerMap.forEach((data, name) => {
      if (name.toLowerCase().includes(searchTerm)) {
        const alternateNamesWithCounts = [];
        data.alternateNames.forEach((count, name) => {
          alternateNamesWithCounts.push({ name: name, count: count });
        });

        matches.push({
          name: name,
          gamesPlayed: data.gamesPlayed,
          alternateNames: Array.from(data.alternateNames.keys()),
          alternateNamesWithCounts: alternateNamesWithCounts,
          rating: data.rating,
          infractionCount: data.infractionCount,
          isSmurf: data.infractionCount > 0,
          smurfLevel: data.rating === 50 ? 'clean' : 
                     data.rating >= 36 ? 'possible' : 'likely',
          infractionDemos: data.infractionDemos || []
        });
      }
    });

    matches.sort((a, b) => b.gamesPlayed - a.gamesPlayed);

    debug.level2('Smurf check complete:', { 
      searchTerm: playerName, 
      matchesFound: matches.length,
      smurfsFound: matches.filter(m => m.isSmurf).length
    });
    
    debug.exit('smurfCheck', startTime, { matchesFound: matches.length }, 1);
    
    res.json({
      searchTerm: playerName,
      results: matches,
      totalMatches: matches.length
    });

  } catch (error) {
    debug.error('smurfCheck', error, 1);
    debug.exit('smurfCheck', startTime, 'error', 1);
    res.status(500).json({ error: 'Unable to perform smurf check' });
  }
});

router.post('/api/smurf-report', authenticateToken, async (req, res) => {
  const startTime = debug.enter('smurfReport', [req.body.playerName, req.user.username], 1);
  const { playerName, description } = req.body;
  const reporterUserId = req.user.id;
  const reporterUsername = req.user.username;

  debug.level2('Smurf report submission:', { playerName, reporterUsername });

  if (!playerName || !description) {
    debug.level1('Smurf report missing required fields');
    debug.exit('smurfReport', startTime, 'missing_fields', 1);
    return res.status(400).json({ error: 'Player name and description are required' });
  }

  try {
    debug.dbQuery('SELECT email FROM users WHERE id = ?', [reporterUserId], 2);
    const [userResult] = await pool.query('SELECT email FROM users WHERE id = ?', [reporterUserId]);
    debug.dbResult(userResult, 2);

    if (userResult.length === 0) {
      debug.level1('Reporter user not found:', reporterUserId);
      debug.exit('smurfReport', startTime, 'user_not_found', 1);
      return res.status(401).json({ error: 'User not found' });
    }

    const reporterEmail = userResult[0].email;
    const emailContent = `
      <h2>New Smurf Report Submitted</h2>
      <p><strong>Player Name in Question:</strong> ${playerName}</p>
      <p><strong>Description:</strong></p>
      <p>${description.replace(/\n/g, '<br>')}</p>
      
      <hr>
      <p><strong>Report submitted by:</strong></p>
      <p><strong>Username:</strong> ${reporterUsername}</p>
      <p><strong>Email:</strong> ${reporterEmail}</p>
      <p><strong>User ID:</strong> ${reporterUserId}</p>
      
      <hr>
      <p><em>This report was submitted through the DefconExpanded Smurf Checker system.</em></p>
    `;

    debug.level2('Sending smurf report email to admin');
    await transporter.sendMail({
      from: '"DefconExpanded Smurf Reports" <keiron.mcphee1@gmail.com>',
      to: 'keiron.mcphee1@gmail.com',
      subject: `Smurf Report: ${playerName} (by ${reporterUsername})`,
      html: emailContent
    });

    debug.level2('Smurf report email sent successfully');
    debug.exit('smurfReport', startTime, 'success', 1);
    res.json({ message: 'Report submitted successfully.' });

  } catch (error) {
    debug.error('smurfReport', error, 1);
    debug.exit('smurfReport', startTime, 'error', 1);
    res.status(500).json({ error: 'Unable to submit report. Please try again later.' });
  }
});

module.exports = router; 