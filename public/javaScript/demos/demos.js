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
//Last Edited 01-04-2025

import {
    initializeDemos,
    performPlayerSearch,
    resetFilters,
    allDemos,
    currentPage,
    totalPages
} from './filters.js';
import { toggleSpectators } from './democard.js';
import { showReportOptions } from '../main/reporting.js';

window.toggleSpectators = toggleSpectators;
window.showReportOptions = showReportOptions;
window.performPlayerSearch = performPlayerSearch;
window.resetFilters = resetFilters;

export {
    initializeDemos,
    allDemos,
    currentPage,
    totalPages,
};