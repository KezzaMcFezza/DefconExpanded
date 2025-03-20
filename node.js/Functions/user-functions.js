const { pool } = require('../Utils/shared-utils');

async function getUserLikesAndFavorites(userId) {
    try {
        const [likes] = await pool.query('SELECT mod_id FROM mod_likes WHERE user_id = ?', [userId]);
        const [favorites] = await pool.query('SELECT mod_id FROM mod_favorites WHERE user_id = ?', [userId]);

        return {
            likes: likes.map(like => like.mod_id),
            favorites: favorites.map(favorite => favorite.mod_id)
        };
    } catch (error) {
        console.error('Error fetching user likes and favorites:', error);
        return { likes: [], favorites: [] };
    }
}

// Helper function to check if a user has liked a specific mod
async function hasUserLikedMod(userId, modId) {
    try {
        const [rows] = await pool.query('SELECT * FROM mod_likes WHERE user_id = ? AND mod_id = ?', [userId, modId]);
        return rows.length > 0;
    } catch (error) {
        console.error('Error checking if user liked mod:', error);
        return false;
    }
}

// Helper function to check if a user has favorited a specific mod
async function hasUserFavoritedMod(userId, modId) {
    try {
        const [rows] = await pool.query('SELECT * FROM mod_favorites WHERE user_id = ? AND mod_id = ?', [userId, modId]);
        return rows.length > 0;
    } catch (error) {
        console.error('Error checking if user favorited mod:', error);
        return false;
    }
}

// Function to add a like to a mod
async function addModLike(userId, modId) {
    try {
        await pool.query('INSERT INTO mod_likes (user_id, mod_id) VALUES (?, ?)', [userId, modId]);
        await pool.query('UPDATE mods SET likes = likes + 1 WHERE id = ?', [modId]);
        return true;
    } catch (error) {
        console.error('Error adding mod like:', error);
        return false;
    }
}

// Function to remove a like from a mod
async function removeModLike(userId, modId) {
    try {
        await pool.query('DELETE FROM mod_likes WHERE user_id = ? AND mod_id = ?', [userId, modId]);
        await pool.query('UPDATE mods SET likes = GREATEST(likes - 1, 0) WHERE id = ?', [modId]);
        return true;
    } catch (error) {
        console.error('Error removing mod like:', error);
        return false;
    }
}

// Function to add a favorite to a mod
async function addModFavorite(userId, modId) {
    try {
        await pool.query('INSERT INTO mod_favorites (user_id, mod_id) VALUES (?, ?)', [userId, modId]);
        await pool.query('UPDATE mods SET favorites = favorites + 1 WHERE id = ?', [modId]);
        return true;
    } catch (error) {
        console.error('Error adding mod favorite:', error);
        return false;
    }
}

// Function to remove a favorite from a mod
async function removeModFavorite(userId, modId) {
    try {
        await pool.query('DELETE FROM mod_favorites WHERE user_id = ? AND mod_id = ?', [userId, modId]);
        await pool.query('UPDATE mods SET favorites = GREATEST(favorites - 1, 0) WHERE id = ?', [modId]);
        return true;
    } catch (error) {
        console.error('Error removing mod favorite:', error);
        return false;
    }
}

// Function to get user's mod interaction history
async function getUserModHistory(userId) {
    try {
        const [likes] = await pool.query(`
            SELECT m.* 
            FROM mods m 
            JOIN mod_likes ml ON m.id = ml.mod_id 
            WHERE ml.user_id = ?
        `, [userId]);

        const [favorites] = await pool.query(`
            SELECT m.* 
            FROM mods m 
            JOIN mod_favorites mf ON m.id = mf.mod_id 
            WHERE mf.user_id = ?
        `, [userId]);

        return {
            likes,
            favorites
        };
    } catch (error) {
        console.error('Error fetching user mod history:', error);
        return { likes: [], favorites: [] };
    }
}

// Function to get mod interaction counts
async function getModInteractionCounts(modId) {
    try {
        const [likes] = await pool.query('SELECT COUNT(*) as count FROM mod_likes WHERE mod_id = ?', [modId]);
        const [favorites] = await pool.query('SELECT COUNT(*) as count FROM mod_favorites WHERE mod_id = ?', [modId]);

        return {
            likes: likes[0].count,
            favorites: favorites[0].count
        };
    } catch (error) {
        console.error('Error fetching mod interaction counts:', error);
        return { likes: 0, favorites: 0 };
    }
}

module.exports = {
    getUserLikesAndFavorites,
    hasUserLikedMod,
    hasUserFavoritedMod,
    addModLike,
    removeModLike,
    addModFavorite,
    removeModFavorite,
    getUserModHistory,
    getModInteractionCounts
};