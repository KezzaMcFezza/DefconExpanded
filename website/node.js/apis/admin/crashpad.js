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
const multer = require('multer');

const debug = require('../../debug-helpers');

// crashpad storage for multer
const crashpadStorage = multer.diskStorage({
    destination: (req, file, cb) => {
        if (req.crashpadFolder) {
            cb(null, req.crashpadFolder);
            return;
        }
        
        const now = new Date();
        const day = String(now.getDate()).padStart(2, '0');
        const month = String(now.getMonth() + 1).padStart(2, '0');
        const year = now.getFullYear();
        const hours = String(now.getHours()).padStart(2, '0');
        const minutes = String(now.getMinutes()).padStart(2, '0');
        const folderName = `crash_${day}-${month}-${year}-${hours}-${minutes}`;
        
        let crashDir = path.join(__dirname, "..", "..", "crashpad-dumps", folderName);
        let counter = 0;
        const seconds = String(now.getSeconds()).padStart(2, '0');
        while (fs.existsSync(crashDir)) {
            if (counter === 0) {
                crashDir = path.join(__dirname, "..", "..", "crashpad-dumps", `${folderName}-${seconds}`);
            } else {
                crashDir = path.join(__dirname, "..", "..", "crashpad-dumps", `${folderName}-${seconds}-${counter}`);
            }
            counter++;
        }
        
        if (!fs.existsSync(crashDir)) {
            fs.mkdirSync(crashDir, { recursive: true });
        }
        
        req.crashpadFolder = crashDir;
        req.crashpadFolderName = path.basename(crashDir);
        
        cb(null, crashDir);
    },
    filename: (req, file, cb) => {
        // keep original filename for all files
        cb(null, file.originalname || file.fieldname);
    }
});

const crashpadUpload = multer({ 
    storage: crashpadStorage,
    limits: { fileSize: 100 * 1024 * 1024 } // 100MB limit
});

// the crashpad route
router.post('/api/defcon-crashpad', crashpadUpload.any(), (req, res) => {
    const startTime = debug.enter('crashpadUpload', [], 1);
    console.log('Crashpad report received');
    
    const metadata = req.body || {};
    const files = req.files || [];

    const minidumpFile = files.find(f => 
        f.fieldname === 'upload_file_minidump' || 
        (f.originalname && f.originalname.endsWith('.dmp')) ||
        (f.filename && f.filename.endsWith('.dmp'))
    );
    
    if (!minidumpFile) {
        debug.level1('Crashpad upload rejected: No minidump file received');
        debug.exit('crashpadUpload', startTime, 'no_minidump', 1);
        return res.status(400).json({
            status: "error",
            message: "No minidump file received"
        });
    }

    let descriptionContent = "";

    // create description.txt with annotations
    const crashDir = req.crashpadFolder;
    if (crashDir) {
        const descriptionPath = path.join(crashDir, 'description.txt');
        
        Object.keys(metadata).forEach(key => {
            if (key !== 'upload_file_minidump' && !key.startsWith('upload_file_')) {
                descriptionContent += `${key}: ${metadata[key]}\n`;
            }
        });
        
        files.forEach(file => {
            descriptionContent += `${file.originalname || file.filename}\n`;
        });
        
        fs.writeFileSync(descriptionPath, descriptionContent, 'utf8');
        debug.level3('Created description.txt in crash folder');
    }

    debug.level2(`Crashpad files saved to folder: ${req.crashpadFolderName || 'unknown'}`);
    console.log(`Total files received: ${files.length}`);
    debug.exit('crashpadUpload', startTime, { folder: req.crashpadFolderName, fileCount: files.length }, 1);

    res.json({
        status: "ok",
        message: "Crash received",
        folder: req.crashpadFolderName,
        minidump: minidumpFile.originalname || minidumpFile.filename,
        files: files.map(f => f.originalname || f.filename),
        metadata
    });
});

module.exports = router;

