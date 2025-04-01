import React, { useState, useEffect } from 'react';
import { Link, useLocation } from 'react-router-dom';
import { useAuth } from '../AuthContext';

// Sidebar/Navigation component
export function Sidebar() {
  const location = useLocation();
  const { user, isAuthenticated, logout } = useAuth();
  const [mobileMenuOpen, setMobileMenuOpen] = useState(false);
  const [openDropdowns, setOpenDropdowns] = useState([]);

  // Toggle mobile menu
  const toggleMobileMenu = (e) => {
    e.preventDefault();
    setMobileMenuOpen(!mobileMenuOpen);
  };

  // Toggle dropdown in sidebar
  const toggleDropdown = (index, e) => {
    e.preventDefault();
    
    setOpenDropdowns(prev => {
      if (prev.includes(index)) {
        return prev.filter(i => i !== index);
      } else {
        return [...prev, index];
      }
    });
  };

  // Close menu when clicking a direct link
  const handleLinkClick = () => {
    setMobileMenuOpen(false);
  };

  // Set active nav item based on current path
  useEffect(() => {
    const currentPath = location.pathname;
    
    // Set active dropdowns based on current path
    const activeDropdowns = [];
    
    if (currentPath.includes('/about')) activeDropdowns.push(0);
    if (currentPath.includes('/modlist')) activeDropdowns.push(1);
    
    setOpenDropdowns(activeDropdowns);
    
  }, [location.pathname]);

  return (
    <div id="sidebar">
      <div className="title" onClick={toggleMobileMenu}>
        <h2>DefconExpanded</h2>
        <i className={`fas fa-bars menu-icon ${mobileMenuOpen ? 'open' : ''}`}></i>
      </div>
      
      <ul className={`list-items ${mobileMenuOpen ? 'show' : ''}`}>
        <li className={location.pathname === '/' ? 'active' : ''}>
          <Link to="/" onClick={handleLinkClick}>Home</Link>
        </li>
        
        <li className={`dropdown ${location.pathname.includes('/about') ? 'active' : ''}`}>
          <a href="#" onClick={(e) => toggleDropdown(0, e)}>
            About <i className="fas fa-chevron-down"></i>
          </a>
          <div className={`dropdown-content ${openDropdowns.includes(0) ? 'show' : ''}`}>
            <Link to="/about" onClick={handleLinkClick}>About</Link>
            <Link to="/about/combined-servers" onClick={handleLinkClick}>Combined Servers</Link>
            <Link to="/about/hours-played" onClick={handleLinkClick}>Hours Played</Link>
            <Link to="/about/popular-territories" onClick={handleLinkClick}>Popular Territories</Link>
            <Link to="/about/1v1-setup-statistics" onClick={handleLinkClick}>1v1 Setup Statistics</Link>
          </div>
        </li>
        
        <li className={location.pathname === '/resources' ? 'active' : ''}>
          <Link to="/resources" onClick={handleLinkClick}>Resources</Link>
        </li>
        
        <li className={location.pathname === '/dedcon-builds' ? 'active' : ''}>
          <Link to="/dedcon-builds" onClick={handleLinkClick}>Dedcon Builds</Link>
        </li>
        
        <li className={`dropdown ${location.pathname.includes('/modlist') ? 'active' : ''}`}>
          <a href="#" onClick={(e) => toggleDropdown(1, e)}>
            Mods <i className="fas fa-chevron-down"></i>
          </a>
          <div className={`dropdown-content ${openDropdowns.includes(1) ? 'show' : ''}`}>
            <Link to="/modlist" onClick={handleLinkClick}>All Mods</Link>
            <Link to="/modlist/maps" onClick={handleLinkClick}>Maps</Link>
            <Link to="/modlist/graphics" onClick={handleLinkClick}>Graphics</Link>
            <Link to="/modlist/overhauls" onClick={handleLinkClick}>Overhauls</Link>
            <Link to="/modlist/moddingtools" onClick={handleLinkClick}>Modding Tools</Link>
            <Link to="/modlist/ai" onClick={handleLinkClick}>AI Mods</Link>
          </div>
        </li>
        
        {/* Auth actions */}
        {isAuthenticated ? (
          <li className="logged-in-actions">
            <span id="logged-in-username">{user?.username}</span>
            <Link to={`/profile/${user?.username}`} className="profile-link">My Profile</Link>
            <button onClick={logout} className="signout">Sign Out</button>
          </li>
        ) : (
          <li className="logged-out-actions">
            <Link to="/login" onClick={handleLinkClick}>Login</Link>
            <Link to="/signup" onClick={handleLinkClick}>Sign Up</Link>
          </li>
        )}
      </ul>
    </div>
  );
}

// Dialog component (to replace your global alert/confirm)
export function Dialog({ 
  isOpen, 
  onClose, 
  onConfirm, 
  message, 
  confirmText = "OK", 
  cancelText = "Cancel",
  type = "confirm" // "confirm" or "alert"
}) {
  if (!isOpen) return null;
  
  return (
    <div className="custom-dialog" style={{ display: 'block' }}>
      <div className="dialog-content">
        <label id="dialog-message">{message}</label>
        <div className="dialog-buttons">
          {type === "confirm" && (
            <button 
              id="dialog-cancel" 
              className="dialog-button"
              onClick={onClose}
            >
              {cancelText}
            </button>
          )}
          <button 
            id="dialog-confirm" 
            className="dialog-button"
            onClick={() => {
              onConfirm && onConfirm();
              onClose();
            }}
          >
            {confirmText}
          </button>
        </div>
      </div>
    </div>
  );
}

// Main layout component
export function MainLayout({ children }) {
  return (
    <div className="page-container">
      <Sidebar />
      <div className="main-content">
        {children}
      </div>
    </div>
  );
}