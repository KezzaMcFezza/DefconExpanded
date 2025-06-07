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
    const [demos] = await pool.query('SELECT id, name, players, game_type, date FROM demos ORDER BY date DESC');
    const playerStats = new Map(); 
    const keyIdToNames = new Map(); 
    const ipToNames = new Map(); 
    const parsedDemos = new Map(); 
    
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
        
        parsedDemos.set(demo.id, { demo, players });
        
        players.forEach(player => {
          if (!player.name) return;

          if (!playerStats.has(player.name)) {
            playerStats.set(player.name, {
              gamesPlayed: 0,
              alternateNames: new Map(),
              infractionDemos: []
            });
          }
          
          playerStats.get(player.name).gamesPlayed++;

          if (player.key_id && player.key_id.trim() !== '' && player.key_id !== 'DEMO') {
            if (!keyIdToNames.has(player.key_id)) {
              keyIdToNames.set(player.key_id, new Set());
            }
            keyIdToNames.get(player.key_id).add(player.name);
          }

          if (player.ip && player.ip.trim() !== '') {
            if (!ipToNames.has(player.ip)) {
              ipToNames.set(player.ip, new Set());
            }
            ipToNames.get(player.ip).add(player.name);
          }
        });
      } catch (error) {
        console.error('Error processing demo:', error);
      }
    });

    const nameToAlternates = new Map(); 
    const nameToInfractionDemos = new Map();
    
    for (const [keyId, names] of keyIdToNames) {
      if (names.size > 1) {
        const nameArray = Array.from(names);
        for (const name of nameArray) {
          if (!nameToAlternates.has(name)) {
            nameToAlternates.set(name, new Set());
          }
          if (!nameToInfractionDemos.has(name)) {
            nameToInfractionDemos.set(name, new Map());
          }
          
          for (const alternateName of nameArray) {
            if (alternateName !== name) {
              nameToAlternates.get(name).add(alternateName);
            }
          }
          
          for (const alternateName of nameArray) {
            if (alternateName !== name) {
              for (const [demoId, demoData] of parsedDemos) {
                const hasAlternateWithKeyId = demoData.players.some(p => p.name === alternateName && p.key_id === keyId);
                if (hasAlternateWithKeyId) {
                  nameToInfractionDemos.get(name).set(demoId, {
                    id: demoData.demo.id,
                    name: demoData.demo.name,
                    game_type: demoData.demo.game_type,
                    date: demoData.demo.date,
                    duration: demoData.demo.duration,
                    players: demoData.demo.players,
                    spectators: demoData.demo.spectators,
                    download_count: demoData.demo.download_count,
                    reason: `Alternate name detected: ${alternateName}`,
                    detectionType: 'authentication'
                  });
                }
              }
            }
          }
        }
      }
    }
    
    for (const [ip, names] of ipToNames) {
      if (names.size > 1) {
        const nameArray = Array.from(names);
        for (const name of nameArray) {
          if (!nameToAlternates.has(name)) {
            nameToAlternates.set(name, new Set());
          }
          if (!nameToInfractionDemos.has(name)) {
            nameToInfractionDemos.set(name, new Map());
          }
          
          for (const alternateName of nameArray) {
            if (alternateName !== name) {
              nameToAlternates.get(name).add(alternateName);
            }
          }
          
          for (const alternateName of nameArray) {
            if (alternateName !== name) {
              for (const [demoId, demoData] of parsedDemos) {
                const hasAlternateWithIp = demoData.players.some(p => p.name === alternateName && p.ip === ip);
                if (hasAlternateWithIp) {
                  nameToInfractionDemos.get(name).set(demoId, {
                    id: demoData.demo.id,
                    name: demoData.demo.name,
                    game_type: demoData.demo.game_type,
                    date: demoData.demo.date,
                    duration: demoData.demo.duration,
                    players: demoData.demo.players,
                    spectators: demoData.demo.spectators,
                    download_count: demoData.demo.download_count,
                    reason: `Alternate name detected: ${alternateName}`,
                    detectionType: 'network'
                  });
                }
              }
            }
          }
        }
      }
    }

         for (const [name, alternateNames] of nameToAlternates) {
       const playerData = playerStats.get(name);
       if (!playerData) continue;
       
       const allInfractionDemos = new Map();
       let totalExpectedEvidenceGames = 0;
       
       for (const alternateName of alternateNames) {
         let count = 0;
         for (const [demoId, demoData] of parsedDemos) {
           if (demoData.players.some(p => p.name === alternateName)) {
             count++;
             totalExpectedEvidenceGames++;
             
             if (!allInfractionDemos.has(demoId)) {
               allInfractionDemos.set(demoId, {
                 id: demoData.demo.id,
                 name: demoData.demo.name,
                 game_type: demoData.demo.game_type,
                 date: demoData.demo.date,
                 duration: demoData.demo.duration,
                 players: demoData.demo.players,
                 spectators: demoData.demo.spectators,
                 download_count: demoData.demo.download_count,
                 reason: `Alternate name detected: ${alternateName}`,
                 detectionType: 'occurrence'
               });
             }
           }
         }
         playerData.alternateNames.set(alternateName, count);
       }
       
       playerData.infractionDemos = Array.from(allInfractionDemos.values())
         .sort((a, b) => new Date(b.date) - new Date(a.date))
         .slice(0, 100);
     }

    const searchTerm = playerName.toLowerCase();
    const matches = [];

    for (const [name, data] of playerStats) {
      if (name.toLowerCase().includes(searchTerm)) {
        const alternateNamesWithCounts = [];
        let totalOccurrences = 0;
        
        data.alternateNames.forEach((count, name) => {
          alternateNamesWithCounts.push({ name: name, count: count });
          totalOccurrences += count;
        });

        const infractionCount = data.alternateNames.size;
        const rating = Math.max(0, 50 - (infractionCount * 7));
        const evidenceGameCount = (data.infractionDemos || []).length;

        debug.level3(`Search result for "${name}": ${infractionCount} alternate names, ${totalOccurrences} total occurrences, ${evidenceGameCount} evidence games shown`);
        
        if (alternateNamesWithCounts.length > 0) {
          debug.level3(`  Alternate names breakdown:`);
          alternateNamesWithCounts.forEach(item => {
            debug.level3(`    - ${item.name}: appears in ${item.count} games`);
          });
        }

        matches.push({
          name: name,
          gamesPlayed: data.gamesPlayed,
          alternateNames: Array.from(data.alternateNames.keys()),
          alternateNamesWithCounts: alternateNamesWithCounts,
          rating: rating,
          infractionCount: infractionCount,
          isSmurf: infractionCount > 0,
          smurfLevel: rating === 50 ? 'clean' : 
                     rating >= 36 ? 'possible' : 'likely',
          infractionDemos: data.infractionDemos || []
        });
      }
    }

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