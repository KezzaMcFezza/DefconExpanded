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

const path = require('path');
const discordState = { isReady: false }; 
// turns out booleans in javascript are passed on by value not reference so i had to get creative
// to allow the whole node app to have a shared constant such as the discord bot, we need to add a constant that can actually be used across the whole app and be exported.
// exporting it like this... const discordState = False will not work here.

const { 
    Client, 
    GatewayIntentBits, 
    EmbedBuilder 
} = require('discord.js');

const {
    DISCORD_CONFIG,
    publicDir
} = require('./constants');

let pendingInitialization = null;
let completePendingInitialization = null;

const discordBot = new Client({
    intents: [
        GatewayIntentBits.Guilds,
        GatewayIntentBits.GuildMessages
    ]
});

discordBot.once('ready', () => {
    console.log(`Discord bot logged in as ${discordBot.user.tag}`);
    discordState.isReady = true; 
    if (pendingInitialization) {
        console.log('Discord bot ready, proceeding with pending initialization...');
        completePendingInitialization();
    }
});

discordBot.login(DISCORD_CONFIG.token);

async function sendDemoToDiscord(demo, logData) {
    if (!discordState.isReady) {
        console.log('Discord bot not ready, waiting...');
        await new Promise((resolve) => {
            const checkInterval = setInterval(() => {
                if (!discordState.isReady) {
                    clearInterval(checkInterval);
                    resolve();
                }
            }, 1000);
        });
    }

    try {
        for (const channelId of DISCORD_CONFIG.channelIds) {
            try {
                const channel = await discordBot.channels.fetch(channelId);
                if (!channel) {
                    console.log(`Could not find channel with ID: ${channelId}`);
                    continue;
                }

                const vanillaAllianceColors = {
                    0: { color: '#ff4949', name: 'Red', emoji: '🔴' },
                    1: { color: '#00bf00', name: 'Green', emoji: '🟢' },
                    2: { color: '#3d5cff', name: 'Blue', emoji: '🔵' },
                    3: { color: '#e5cb00', name: 'Yellow', emoji: '🟡' },
                    4: { color: '#ffa700', name: 'Orange', emoji: '🟠' },
                    5: { color: '#00e5ff', name: 'Turq', emoji: '🔷' }
                };

                const expandedAllianceColors = {
                    0: { color: '#00bf00', name: 'Green', emoji: '🟢' },
                    1: { color: '#ff4949', name: 'Red', emoji: '🔴' },
                    2: { color: '#3d5cff', name: 'Blue', emoji: '🔵' },
                    3: { color: '#e5cb00', name: 'Yellow', emoji: '🟡' },
                    4: { color: '#00e5ff', name: 'Turq', emoji: '🔷' },
                    5: { color: '#e72de0', name: 'Pink', emoji: '🟣' },
                    6: { color: '#4c4c4c', name: 'Black', emoji: '⚫' },
                    7: { color: '#ffa700', name: 'Orange', emoji: '🟠' },
                    8: { color: '#28660a', name: 'Olive', emoji: '🟢' },
                    9: { color: '#660011', name: 'Scarlet', emoji: '🔴' },
                    10: { color: '#2a00ff', name: 'Indigo', emoji: '🔵' },
                    11: { color: '#4c4c00', name: 'Gold', emoji: '🟡' },
                    12: { color: '#004c3e', name: 'Teal', emoji: '🔷' },
                    13: { color: '#6a007f', name: 'Purple', emoji: '🟣' },
                    14: { color: '#e5e5e5', name: 'White', emoji: '⚪' },
                    15: { color: '#964B00', name: 'Brown', emoji: '🟤' }
                };

                const isExpandedServer = (logData.players && logData.players.length > 6) ||
                    demo.game_type.includes('8 Player') ||
                    demo.game_type.includes('4v4') ||
                    demo.game_type.includes('10 Player') ||
                    demo.game_type.includes('5v5') ||
                    demo.game_type.includes('16 Player') ||
                    demo.game_type.includes('8v8') ||
                    demo.game_type.includes('509') ||
                    demo.game_type.includes('CG') ||
                    demo.game_type.includes('MURICON');

                const allianceColors = isExpandedServer ? expandedAllianceColors : vanillaAllianceColors;

                const territoryImages = {
                    'North America': 'northamerica',
                    'South America': 'southamerica',
                    'Europe': 'europe',
                    'Africa': 'africa',
                    'Asia': 'asia',
                    'Russia': 'russia',
                    'North Africa': 'northafrica',
                    'South Africa': 'southafrica',
                    'East Asia': 'eastasia',
                    'West Asia': 'westasia',
                    'South Asia': 'southasia',
                    'Australasia': 'australasia',
                    'Antartica': 'antartica'
                };

                const embed = new EmbedBuilder()
                    .setColor('#0099ff')
                    .setTitle(demo.game_type || 'Defcon Game')
                    .setDescription('---------------------------------------------------------------------');

                if (demo.duration) {
                    embed.addFields({
                        name: 'Game Duration',
                        value: demo.duration.split('.')[0],
                        inline: true
                    });
                }

                if (demo.date) {
                    embed.addFields({
                        name: 'Date',
                        value: new Date(demo.date).toLocaleString(),
                        inline: true
                    });
                }

                if (logData.players && Array.isArray(logData.players)) {
                    let playerGroups = {};

                    logData.players.forEach(player => {
                        if (player.alliance !== undefined) {
                            playerGroups[player.alliance] = playerGroups[player.alliance] || [];
                            playerGroups[player.alliance].push(player);
                        }
                    });

                    const sortedGroups = Object.values(playerGroups).sort((a, b) => {
                        const scoreA = a.reduce((sum, p) => sum + (p.score || 0), 0);
                        const scoreB = b.reduce((sum, p) => sum + (p.score || 0), 0);
                        return scoreB - scoreA;
                    });

                    sortedGroups.forEach(group => {
                        const sortedPlayers = group.sort((a, b) => (b.score || 0) - (a.score || 0));
                        const teamColor = allianceColors[sortedPlayers[0].alliance] || allianceColors[0];

                        const playersText = sortedPlayers.map(player =>
                            `${player.name} | ${player.territory} = ${player.score || 0}`
                        ).join('\n');

                        embed.addFields({
                            name: `${teamColor.emoji} ${teamColor.name}`,
                            value: playersText,
                            inline: false
                        });
                    });

                    if (sortedGroups.length >= 2) {
                        const winningGroup = sortedGroups[0];
                        const runnerUpGroup = sortedGroups[1];
                        const winningScore = winningGroup.reduce((sum, p) => sum + (p.score || 0), 0);
                        const runnerUpScore = runnerUpGroup.reduce((sum, p) => sum + (p.score || 0), 0);
                        const scoreDifference = winningScore - runnerUpScore;

                        const winningTeamColor = allianceColors[winningGroup[0].alliance] || allianceColors[0];
                        embed.addFields({
                            name: '\u200b',
                            value: `**${winningTeamColor.name}** won by ${scoreDifference} points`,
                            inline: false
                        });
                    }
                }

                if (demo.name) {
                    embed.addFields({
                        name: 'Download',
                        value: `[Click to download](${process.env.BASE_URL}/api/download/${demo.name})`,
                        inline: true
                    });
                }

                const spectatorsText = logData.spectators && logData.spectators.length > 0
                    ? logData.spectators
                        .filter(s => s.name)
                        .map(s => s.name)
                        .join(', ') || 'No spectators'
                    : 'No spectators';

                embed.addFields({
                    name: 'Spectators',
                    value: spectatorsText,
                    inline: true
                });

                try {
                    const mapBuffer = await createTerritoryMap(logData.players, territoryImages, false, allianceColors, allianceColors);
                    if (mapBuffer) {
                        embed.setImage('attachment://map.png');
                        await channel.send({
                            embeds: [embed],
                            files: [{
                                attachment: mapBuffer,
                                name: 'map.png'
                            }]
                        });
                    } else {
                        await channel.send({ embeds: [embed] });
                    }
                } catch (mapError) {
                    console.error('Error generating territory map:', mapError);
                    await channel.send({ embeds: [embed] });
                }

            } catch (channelError) {
                console.error(`Error sending to channel ${channelId}:`, channelError);
                continue;
            }
        }
    } catch (error) {
        console.error('Error sending demo to Discord:', error);
    }
}

async function createTerritoryMap(players, territoryImages, usingAlliances, teamColors, allianceColors) {
    try {
        const { createCanvas, loadImage } = require('canvas');
        const canvas = createCanvas(800, 400);
        const ctx = canvas.getContext('2d');

        const baseMap = await loadImage(path.join(publicDir, 'images', 'base_map.png'));
        ctx.drawImage(baseMap, 0, 0, 800, 400);

        ctx.globalAlpha = 0.7;

        for (const player of players) {
            const territory = territoryImages[player.territory];
            if (!territory) continue;

            const colorSystem = allianceColors;
            const color = colorSystem[player.alliance]?.color || '#00bf00';
            const colorHex = color.replace('#', '');

            try {
                const overlayPath = path.join(publicDir, 'images', `${territory}${colorHex}.png`);
                const overlay = await loadImage(overlayPath);
                ctx.drawImage(overlay, 0, 0, 800, 400);
            } catch (err) {
                console.error(`Error loading territory overlay for ${territory}:`, err);
            }
        }

        ctx.globalAlpha = 1.0;

        return canvas.toBuffer();
    } catch (error) {
        console.error('Error creating territory map:', error);
        return null;
    }
}

module.exports = {
    discordBot,
    sendDemoToDiscord,
    createTerritoryMap,
    discordState
};