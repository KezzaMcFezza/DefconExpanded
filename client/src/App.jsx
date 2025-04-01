import React from 'react';
import { BrowserRouter, Routes, Route } from 'react-router-dom';
import { AuthProvider } from './AuthContext';
import { MainLayout } from './components/Navigation';
import ModList from './features/ModList';

// These would be your page components, implemented separately
import Home from './pages/Home';
import About from './pages/About';
import CombinedServers from './pages/CombinedServers';
import Resources from './pages/Resources';
import DedconBuilds from './pages/DedconBuilds';
import Login from './pages/Login';
import Signup from './pages/Signup';
import Profile from './pages/Profile';

function App() {
  return (
    <AuthProvider>
      <BrowserRouter>
        <MainLayout>
          <Routes>
            <Route path="/" element={<Home />} />
            <Route path="/about" element={<About />} />
            <Route path="/about/combined-servers" element={<CombinedServers />} />
            <Route path="/resources" element={<Resources />} />
            <Route path="/dedcon-builds" element={<DedconBuilds />} />
            
            {/* Mod routes */}
            <Route path="/modlist" element={<ModList modType="all" />} />
            <Route path="/modlist/maps" element={<ModList modType="maps" />} />
            <Route path="/modlist/graphics" element={<ModList modType="graphics" />} />
            <Route path="/modlist/overhauls" element={<ModList modType="overhauls" />} />
            <Route path="/modlist/moddingtools" element={<ModList modType="moddingtools" />} />
            <Route path="/modlist/ai" element={<ModList modType="ai" />} />
            
            {/* Auth routes */}
            <Route path="/login" element={<Login />} />
            <Route path="/signup" element={<Signup />} />
            <Route path="/profile/:username" element={<Profile />} />
          </Routes>
        </MainLayout>
      </BrowserRouter>
    </AuthProvider>
  );
}

export default App;