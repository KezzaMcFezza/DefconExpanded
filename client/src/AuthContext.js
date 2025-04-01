import React, { createContext, useState, useEffect, useContext } from 'react';

const AuthContext = createContext(null);

export function AuthProvider({ children }) {
  const [user, setUser] = useState(null);
  const [loading, setLoading] = useState(true);

  useEffect(() => {
    checkAuthStatus();
  }, []);

  const checkAuthStatus = async () => {
    try {
      const response = await fetch('/api/current-user');
      const data = await response.json();
      
      if (data.user) {
        setUser(data.user);
      } else {
        setUser(null);
      }
    } catch (error) {
      console.error('Error checking authentication status:', error);
      setUser(null);
    } finally {
      setLoading(false);
    }
  };

  const logout = async () => {
    try {
      const response = await fetch('/api/logout', { method: 'POST' });
      const data = await response.json();
      
      if (data.message === 'Logged out successfully') {
        setUser(null);
        alert("You have been successfully logged out.");
      }
    } catch (error) {
      console.error('Error logging out:', error);
    }
  };

  // Helper function to check user roles
  const hasPermission = (requiredRole) => {
    return user && user.role <= requiredRole;
  };

  return (
    <AuthContext.Provider value={{ 
      user, 
      loading, 
      isAuthenticated: !!user, 
      hasPermission,
      logout, 
      refreshAuth: checkAuthStatus 
    }}>
      {children}
    </AuthContext.Provider>
  );
}

// Custom hook for accessing auth context
export function useAuth() {
  const context = useContext(AuthContext);
  if (context === null) {
    throw new Error('useAuth must be used within an AuthProvider');
  }
  return context;
}