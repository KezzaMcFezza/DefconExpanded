import React, { useState, useEffect } from 'react';
import { useAuth } from '../AuthContext';
import { Dialog } from '../components/Navigation';

// Helper to format file sizes
function formatBytes(bytes, decimals = 2) {
  if (bytes === 0) return '0 Bytes';
  const k = 1024;
  const dm = decimals < 0 ? 0 : decimals;
  const sizes = ['Bytes', 'KB', 'MB', 'GB', 'TB', 'PB', 'EB', 'ZB', 'YB'];
  const i = Math.floor(Math.log(bytes) / Math.log(k));
  return parseFloat((bytes / Math.pow(k, i)).toFixed(dm)) + ' ' + sizes[i];
}

export default function ModList({ modType = 'all' }) {
  const { isAuthenticated } = useAuth();
  const [mods, setMods] = useState([]);
  const [loading, setLoading] = useState(true);
  const [searchTerm, setSearchTerm] = useState('');
  const [sortBy, setSortBy] = useState('latest');
  
  // Dialog states
  const [dialog, setDialog] = useState({
    open: false,
    message: '',
    type: 'alert',
    callback: null
  });
  
  // Report dialog state
  const [reportDialog, setReportDialog] = useState({
    open: false,
    mod: null
  });

  // Load mods on component mount and when sort/filter changes
  useEffect(() => {
    loadMods();
  }, [modType, sortBy]);

  const loadMods = async () => {
    try {
      setLoading(true);
      let url = `/api/mods?type=${encodeURIComponent(modType)}`;
      if (searchTerm) {
        url += `&search=${encodeURIComponent(searchTerm)}`;
      }
      url += `&sort=${encodeURIComponent(sortBy)}`;

      const response = await fetch(url);
      if (!response.ok) {
        throw new Error('Failed to fetch mods');
      }
      
      const data = await response.json();
      setMods(data);
    } catch (error) {
      console.error('Error loading mods:', error);
      showAlert('Failed to load mods. Please try again later.');
    } finally {
      setLoading(false);
    }
  };

  // Dialog helpers
  const showAlert = (message) => {
    setDialog({
      open: true,
      message,
      type: 'alert',
      callback: null
    });
  };

  const showConfirm = (message, callback) => {
    setDialog({
      open: true,
      message,
      type: 'confirm',
      callback
    });
  };
  
  const closeDialog = () => {
    setDialog(prev => ({ ...prev, open: false }));
  };

  // Handler for search button
  const handleSearch = () => {
    loadMods();
  };

  // Handler for like button
  const handleLikeMod = async (modId) => {
    if (!isAuthenticated) {
      showAlert('You need to be logged in to like mods.');
      return;
    }
    
    try {
      const response = await fetch(`/api/mods/${modId}/like`, {
        method: 'POST'
      });
      
      if (response.ok) {
        // Update the mods list to reflect the change
        setMods(prevMods => 
          prevMods.map(mod => {
            if (mod.id === modId) {
              const newLikeStatus = !mod.isLiked;
              return { 
                ...mod, 
                isLiked: newLikeStatus,
                likes_count: newLikeStatus 
                  ? (mod.likes_count || 0) + 1 
                  : Math.max(0, (mod.likes_count || 0) - 1)
              };
            }
            return mod;
          })
        );
      } else {
        const data = await response.json();
        showAlert(data.error || 'Failed to like mod');
      }
    } catch (error) {
      console.error('Error liking mod:', error);
      showAlert('An error occurred while liking the mod');
    }
  };

  // Handler for favorite button
  const handleFavoriteMod = async (modId) => {
    if (!isAuthenticated) {
      showAlert('You need to be logged in to favorite mods.');
      return;
    }
    
    try {
      const response = await fetch(`/api/mods/${modId}/favorite`, {
        method: 'POST'
      });
      
      if (response.ok) {
        // Update the mods list to reflect the change
        setMods(prevMods => 
          prevMods.map(mod => {
            if (mod.id === modId) {
              const newFavoriteStatus = !mod.isFavorited;
              return { 
                ...mod, 
                isFavorited: newFavoriteStatus,
                favorites_count: newFavoriteStatus 
                  ? (mod.favorites_count || 0) + 1 
                  : Math.max(0, (mod.favorites_count || 0) - 1)
              };
            }
            return mod;
          })
        );
      } else {
        const data = await response.json();
        showAlert(data.error || 'Failed to favorite mod');
      }
    } catch (error) {
      console.error('Error favoriting mod:', error);
      showAlert('An error occurred while favoriting the mod');
    }
  };
  
  // Open report dialog
  const openReportDialog = (mod) => {
    if (!isAuthenticated) {
      showAlert('You need to be logged in to report mods.');
      return;
    }
    
    setReportDialog({
      open: true,
      mod
    });
  };
  
  // Submit mod report
  const submitModReport = async (reportType) => {
    const { mod } = reportDialog;
    if (!mod) return;
    
    try {
      const response = await fetch('/api/report-mod', {
        method: 'POST',
        headers: {
          'Content-Type': 'application/json',
        },
        body: JSON.stringify({ modId: mod.id, reportType }),
      });

      if (response.ok) {
        showAlert('Mod report successfully sent to the team!');
      } else {
        throw new Error('Failed to submit report');
      }
    } catch (error) {
      showAlert('An error occurred while submitting your report.');
    } finally {
      setReportDialog({
        open: false,
        mod: null
      });
    }
  };

  // Show loading state
  if (loading) {
    return <div className="loading">Loading mods...</div>;
  }

  return (
    <div className="mod-container">
      {/* Search and sort controls */}
      <div className="mod-filters">
        <div className="search-container">
          <input
            type="text"
            id="mod-search"
            placeholder="Search mods..."
            value={searchTerm}
            onChange={(e) => setSearchTerm(e.target.value)}
            onKeyPress={(e) => e.key === 'Enter' && handleSearch()}
          />
          <button id="search-button" onClick={handleSearch}>Search</button>
        </div>
        
        <div className="sort-container">
          <label htmlFor="sort-select">Sort by:</label>
          <select 
            id="sort-select" 
            value={sortBy} 
            onChange={(e) => setSortBy(e.target.value)}
          >
            <option value="latest">Latest</option>
            <option value="most-downloaded">Most Downloaded</option>
            <option value="most-liked">Most Liked</option>
            <option value="most-favorited">Most Favorited</option>
          </select>
        </div>
      </div>
      
      {/* Mod listing */}
      <div id="mod-list" data-mod-type={modType}>
        {mods.length === 0 ? (
          <div className="no-mods">No mods found matching your criteria</div>
        ) : (
          mods.map(mod => (
            <div className="mod-item" key={mod.id} data-mod-id={mod.id}>
              <div className="mod-header">
                <div 
                  className="mod-header-background" 
                  style={{
                    backgroundImage: `url('${mod.preview_image_path 
                      ? '/' + mod.preview_image_path.split('/').slice(-2).join('/')
                      : '/modpreviews/icon3.png'}')`
                  }}
                ></div>
                <div className="mod-header-overlay"></div>
                <div className="mod-title-container">
                  <h3 className="mod-title">{mod.name}</h3>
                  <h3 className="mod-author">By ({mod.creator})</h3>
                  <span className="file-size">
                    <i className="fas fa-file-archive"></i> {formatBytes(mod.size)}
                  </span>
                  <p className="mod-subtitle">{mod.description}</p>
                  <div className="mod-interaction">
                    <button 
                      className={`like-button ${mod.isLiked ? 'liked' : ''}`}
                      onClick={() => handleLikeMod(mod.id)}
                    >
                      <i className="fas fa-thumbs-up"></i>
                      <span className="like-count">{mod.likes_count || 0}</span>
                    </button>
                    <button 
                      className={`favorite-button ${mod.isFavorited ? 'favorited' : ''}`}
                      onClick={() => handleFavoriteMod(mod.id)}
                    >
                      <i className="fas fa-star"></i>
                      <span className="favorite-count">{mod.favorites_count || 0}</span>
                    </button>
                  </div>
                </div>
              </div>
              <div className="mod-info">
                <div className="mod-meta">
                  <span className="downloads">
                    <i className="fas fa-download"></i> {mod.download_count || 0}
                  </span>
                  <span className="release-date">
                    <i className="fas fa-calendar-alt"></i> {new Date(mod.release_date).toLocaleDateString()}
                  </span>
                  <span className="compatibility">{mod.compatibility || 'Unknown'}</span>
                  <button 
                    className="btn-report"
                    onClick={() => openReportDialog(mod)}
                  >
                    Report
                  </button>
                  <a 
                    href={`/api/download-mod/${mod.id}`} 
                    className="download-btn"
                  >
                    <i className="fas fa-cloud-arrow-down"></i> Download
                  </a>
                </div>
              </div>
            </div>
          ))
        )}
      </div>
      
      {/* Alert/Confirm Dialog */}
      <Dialog
        isOpen={dialog.open}
        onClose={closeDialog}
        onConfirm={() => {
          if (dialog.callback) dialog.callback();
          closeDialog();
        }}
        message={dialog.message}
        type={dialog.type}
      />
      
      {/* Report Dialog */}
      {reportDialog.open && (
        <div className="report-dialog">
          <div className="report-dialog-content">
            <h3>Report Mod: {reportDialog.mod?.name}</h3>
            <p>Please select a reason for your report:</p>
            <div className="report-options">
              {['Broken download', 'Incorrect information', 'Malicious content', 
                'Hard install process', 'Does not work as expected'].map(option => (
                <button 
                  key={option} 
                  className="report-option-btn"
                  onClick={() => submitModReport(option)}
                >
                  {option}
                </button>
              ))}
            </div>
            <div className="report-dialog-buttons">
              <button 
                className="cancel-btn"
                onClick={() => setReportDialog({ open: false, mod: null })}
              >
                Cancel
              </button>
            </div>
          </div>
        </div>
      )}
    </div>
  );
}