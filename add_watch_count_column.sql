-- Add watch_count column to demos table
-- Run this SQL query to add the watch count feature

ALTER TABLE demos ADD COLUMN watch_count INT DEFAULT 0 NOT NULL;

-- Optional: Update existing demos to have a watch_count of 0 (should already be set by DEFAULT)
-- UPDATE demos SET watch_count = 0 WHERE watch_count IS NULL;

-- You can also verify the column was added correctly:
-- DESCRIBE demos; 