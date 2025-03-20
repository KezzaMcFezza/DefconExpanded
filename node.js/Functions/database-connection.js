const mysql = require('mysql2/promise');

// Create a connection pool
const pool = mysql.createPool({
  host: 'localhost',
  user: 'root',
  password: 'cca3d38e2b',
  database: 'defcon_demos',
  connectionLimit: 20,
  queueLimit: 0,
  waitForConnections: true,
  enableKeepAlive: true,
  keepAliveInitialDelay: 10000,
  connectTimeout: 10000,
});

// Simple wrapper for queries to add logging and error handling
const query = async (sql, params = []) => {
  try {
    const [results] = await pool.query(sql, params);
    return results;
  } catch (error) {
    console.error('Database query error:', error);
    console.error('Query:', sql);
    console.error('Parameters:', params);
    throw error;
  }
};

// Transaction wrapper
const transaction = async (callback) => {
  const connection = await pool.getConnection();
  
  try {
    await connection.beginTransaction();
    const result = await callback(connection);
    await connection.commit();
    return result;
  } catch (error) {
    await connection.rollback();
    console.error('Transaction error:', error);
    throw error;
  } finally {
    connection.release();
  }
};

// Check database connection
const checkConnection = async () => {
  try {
    const connection = await pool.getConnection();
    console.log('Successfully connected to the database');
    connection.release();
    return true;
  } catch (error) {
    console.error('Error connecting to the database:', error);
    return false;
  }
};

// Get all rows from a table
const getAll = async (table) => {
  return await query(`SELECT * FROM ${table}`);
};

// Get a single row by ID
const getById = async (table, id) => {
  const rows = await query(`SELECT * FROM ${table} WHERE id = ?`, [id]);
  return rows.length ? rows[0] : null;
};

// Insert a row into a table
const insert = async (table, data) => {
  const columns = Object.keys(data).join(', ');
  const placeholders = Object.keys(data).map(() => '?').join(', ');
  const values = Object.values(data);
  
  const result = await query(
    `INSERT INTO ${table} (${columns}) VALUES (${placeholders})`,
    values
  );
  
  return result.insertId;
};

// Update a row in a table
const update = async (table, id, data) => {
  const setClause = Object.keys(data).map(key => `${key} = ?`).join(', ');
  const values = [...Object.values(data), id];
  
  const result = await query(
    `UPDATE ${table} SET ${setClause} WHERE id = ?`,
    values
  );
  
  return result.affectedRows;
};

// Delete a row from a table
const deleteById = async (table, id) => {
  const result = await query(`DELETE FROM ${table} WHERE id = ?`, [id]);
  return result.affectedRows;
};

// Count rows in a table
const count = async (table, condition = '', params = []) => {
  const whereClause = condition ? `WHERE ${condition}` : '';
  const result = await query(`SELECT COUNT(*) as count FROM ${table} ${whereClause}`, params);
  return result[0].count;
};

module.exports = {
  pool,
  query,
  transaction,
  checkConnection,
  getAll,
  getById,
  insert,
  update,
  deleteById,
  count
};