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
//Last Edited 18-01-2026

// Search for all EDIT ME! strings to replace with your own code.
// This is the HTML template for the replay viewer and sync practice client,
// but it can be used for literally anything else.

const CONFIG = 
{
  PROGRESS_TIMEOUT: 60000,
  SERVICE_WORKER_UPDATE_INTERVAL: 300000,
  LOADING_SCREEN_DELAY: 1500,
  GAME_INIT_CHECK_DELAY: 3000,
  GAME_INIT_CHECK_INTERVAL: 1000,
  IMMEDIATE_TIMER_ID_MIN: 1000000,
  RAF_ID_START: 1000000
};

// ============================================================================
// State Machine for Loading
// ============================================================================

class LoadingStateMachine 
{
  static States = {
    DISCONNECTED: 'disconnected',
    CONNECTING: 'connecting',
    CONNECTED: 'connected',
    ERROR: 'error',
    HIDDEN: 'hidden'
};

  constructor() {
    this.currentState = LoadingStateMachine.States.CONNECTING;
    this.listeners = new Set();
  }

  setState(newState) {
    if (this.currentState !== newState) {
      const oldState = this.currentState;
      this.currentState = newState;
      this.notifyListeners(oldState, newState);
    }
  }

  getState() {
    return this.currentState;
  }

  onStateChange(callback) {
    this.listeners.add(callback);
    return () => this.listeners.delete(callback);
  }

  notifyListeners(oldState, newState) {
    this.listeners.forEach(listener => listener(oldState, newState));
  }
}

// ============================================================================
// Loading UI Manager
// ============================================================================

class LoadingUIManager {
  constructor() {
    this.elements = {
      overlay: document.getElementById('loading-overlay'),
      status: document.getElementById('loading-status'),
      progressBar: document.getElementById('progress-bar'),
      progressBar2: document.getElementById('progress-bar-2'),
      details: document.getElementById('loading-details'),
      info: document.getElementById('loading-info')
    };

    this.state = new LoadingStateMachine();
    this.hasEverLoaded = false;
    this.errorOccurred = false;
    
    this.progressMonitor = null;
    this.lastProgressTime = Date.now();

    this.setupStateListener();
  }

  setupStateListener() {
    this.state.onStateChange((oldState, newState) => {
      this.updateUI(newState);
    });
  }

  updateUI(state) {
    const { status } = this.elements;
    
    status.className = '';
    
    const stateConfig = {
      [LoadingStateMachine.States.DISCONNECTED]: {
        text: 'DISCONNECTED',
        class: 'state-disconnected',
        progress: { percent: 0, color: '#ff0000' }
      },
      [LoadingStateMachine.States.CONNECTING]: {
        text: 'CONNECTING',
        class: 'state-connecting',
        progress: { percent: 50, color: '#ffff00' }
      },
      [LoadingStateMachine.States.CONNECTED]: {
        text: 'CONNECTED',
        class: 'state-connected',
        progress: { percent: 100, color: '#00ff00' }
      },
      [LoadingStateMachine.States.ERROR]: {
        text: 'ERROR',
        class: 'state-error',
        progress: { percent: 0, color: '#ff0000' }
      }
    };

    const config = stateConfig[state];
    if (config) {
      status.textContent = config.text;
      status.classList.add(config.class);
      this.updateProgressBar(config.progress.percent, config.progress.color);
    }
  }

  updateProgressBar(percent, color) {
    if (this.elements.progressBar) {
      this.elements.progressBar.style.width = `${percent}%`;
      this.elements.progressBar.style.backgroundColor = color;
    }
  }

  updateSecondaryProgress(percent, color, text) {
    if (this.elements.progressBar2) {
      this.elements.progressBar2.style.width = `${percent}%`;
      this.elements.progressBar2.style.backgroundColor = color;
    }
    
    if (text && this.elements.details) {
      this.elements.details.textContent = text;
      this.elements.details.classList.toggle('error-text', text === 'ERROR');
      this.elements.details.classList.toggle('loading-text', text === 'LOADING' || text === 'LOADING ASSETS');
      this.elements.details.classList.toggle('received-text', text === 'RECEIVED' || text === 'COMPLETE');
    }
  }

  setInfo(text, isError = false) {
    if (this.elements.info) {
      this.elements.info.textContent = text;
      this.elements.info.classList.toggle('error-text', isError);
    }
  }

  showError(message) {
    if (this.hasEverLoaded) return;
    
    this.errorOccurred = true;
    this.state.setState(LoadingStateMachine.States.ERROR);
    this.setInfo('TRY REFRESHING THE PAGE', true);
    this.updateSecondaryProgress(0, '#ff0000', 'ERROR');
    this.stopProgressMonitoring();
  }

  startProgressMonitoring() {
    this.stopProgressMonitoring();
    
    this.progressMonitor = setTimeout(() => {
      if (!this.hasEverLoaded && !this.errorOccurred) {
        this.showError('NO PROGRESS - CHECK CONNECTION');
      }
    }, CONFIG.PROGRESS_TIMEOUT);
  }

  stopProgressMonitoring() {
    if (this.progressMonitor) {
      clearTimeout(this.progressMonitor);
      this.progressMonitor = null;
    }
  }

  updateProgress(current, total) {
    this.lastProgressTime = Date.now();
    this.startProgressMonitoring();

    const fraction = total > 0 ? (total - current) / total : 0;
    const percent = Math.max(0, Math.min(100, fraction * 100));
    
    this.updateSecondaryProgress(
      percent,
      percent < 100 ? '#ffaa00' : '#00ff00',
      percent < 100 ? 'LOADING' : 'COMPLETE'
    );
  }

  hide() {
    this.hasEverLoaded = true;
    this.stopProgressMonitoring();
    this.state.setState(LoadingStateMachine.States.HIDDEN);
    
    if (this.elements.overlay) {
      this.elements.overlay.classList.add('hidden');
      this.elements.overlay.style.display = 'none';
    }
  }

  forceHide() {
    console.log('Forcing loading screen to hide');
    this.hide();
  }
}

// ============================================================================
// Fullscreen Manager
// ============================================================================

class FullscreenManager {
  constructor(canvasElement) {
    this.canvas = canvasElement;
    this.isFullscreen = false;
    this.eventCleanup = [];
    
    this.init();
  }

  init() {
    const changeHandler = () => this.onFullscreenChange();
    
    const events = [
      'fullscreenchange',
      'webkitfullscreenchange', 
      'mozfullscreenchange',
      'MSFullscreenChange'
    ];

    events.forEach(event => {
      document.addEventListener(event, changeHandler);
      this.eventCleanup.push(() => document.removeEventListener(event, changeHandler));
    });

    this.canvas.addEventListener('click', (e) => this.onClick(e));
    
    const escapeHandler = (e) => this.onEscape(e);
    document.addEventListener('keydown', escapeHandler);
    this.eventCleanup.push(() => document.removeEventListener('keydown', escapeHandler));
  }

  onFullscreenChange() {
    this.isFullscreen = !!(
      document.fullscreenElement ||
      document.webkitFullscreenElement ||
      document.mozFullScreenElement ||
      document.msFullscreenElement
    );

    if (this.isFullscreen) {
      this.canvas.focus();
    } else {
      setTimeout(() => {
        this.canvas.focus();
        this.canvas.getBoundingClientRect();
        window.dispatchEvent(new Event('resize'));
      }, 100);
    }
  }

  onClick(e) {
    if (!this.isFullscreen) {
      this.requestFullscreen();
    }
    e.preventDefault();
    return false;
  }

  onEscape(e) {
    if (e.key === 'Escape' && this.isFullscreen) {
      this.exitFullscreen();
    }
  }

  requestFullscreen() {
    const request = 
      this.canvas.requestFullscreen ||
      this.canvas.webkitRequestFullscreen ||
      this.canvas.mozRequestFullScreen ||
      this.canvas.msRequestFullscreen;

    if (request) {
      request.call(this.canvas);
    }
  }

  exitFullscreen() {
    const exit = 
      document.exitFullscreen ||
      document.webkitExitFullscreen ||
      document.mozCancelFullScreen ||
      document.msExitFullscreen;

    if (exit) {
      exit.call(document);
    }
  }

  destroy() {
    this.eventCleanup.forEach(cleanup => cleanup());
    this.eventCleanup = [];
  }
}

// ============================================================================
// Asset Loader
// ============================================================================

class AssetLoader {
  constructor(uiManager) {
    this.uiManager = uiManager;
    this.totalDependencies = 0;
    this.dataFileLoaded = false;
    this.jsReady = false;
  }

  monitorRunDependencies(remaining) {
    if (this.uiManager.hasEverLoaded) return;

    this.totalDependencies = Math.max(this.totalDependencies, remaining);

    if (remaining > 0) {
      this.uiManager.state.setState(LoadingStateMachine.States.CONNECTING);
      this.uiManager.updateProgress(remaining, this.totalDependencies);
      
      const loaded = this.totalDependencies - remaining;
      this.uiManager.setInfo(`DOWNLOADING... (${loaded}/${this.totalDependencies})`);

      const connectProgress = 50 + ((loaded / this.totalDependencies) * 50);
      this.uiManager.updateProgressBar(connectProgress, '#ffff00');
    } else {
      console.log('JS dependencies completed');
      this.jsReady = true;
      this.uiManager.state.setState(LoadingStateMachine.States.CONNECTED);
      this.uiManager.updateProgressBar(100, '#00ff00');
      this.uiManager.updateSecondaryProgress(100, '#00ff00', 'RECEIVED');
      this.uiManager.setInfo('WAITING FOR GAME DATA...');
      
      this.checkIfFullyLoaded();
    }
  }

  onDataFileLoad() {
    console.log('.data file download completed');
    this.dataFileLoaded = true;
    this.uiManager.updateSecondaryProgress(100, '#00ff00', 'ASSETS LOADED');
    this.uiManager.setInfo('GAME DATA LOADED - CHECKING...');
    this.checkIfFullyLoaded();
  }

  onDataFileProgress(percent) {
    this.uiManager.updateSecondaryProgress(
      percent,
      percent < 100 ? '#ffaa00' : '#00ff00',
      percent < 100 ? 'LOADING ASSETS' : 'ASSETS LOADED'
    );
    this.uiManager.setInfo(`DOWNLOADING GAME DATA... (${Math.round(percent)}%)`);
  }

  onDataFileError() {
    console.error('.data file failed to load');
    this.uiManager.showError('ASSET LOADING FAILED');
  }

  checkIfFullyLoaded() {
    if (this.jsReady && this.dataFileLoaded) {
      this.uiManager.state.setState(LoadingStateMachine.States.CONNECTED);
      this.uiManager.updateSecondaryProgress(100, '#00ff00', 'RECEIVED');
      this.uiManager.setInfo('GAME READY - STARTING...');
      
      setTimeout(() => {
        this.uiManager.hide();
      }, CONFIG.LOADING_SCREEN_DELAY);
    }
  }
}

// ============================================================================
// Service Worker Manager
// ============================================================================

class ServiceWorkerManager {
  constructor() {
    this.registration = null;
  }

  async register() {
    if (!('serviceWorker' in navigator)) {
      console.warn('Service Workers not supported');
      return;
    }

    // EDIT ME!
    // The path to the service worker file

    try {
      this.registration = await navigator.serviceWorker.register(
        'service-worker.js',
        { scope: './' }
      );

      console.log('Service Worker registered successfully');
      
      await this.registration.update();
      
      if (this.registration.waiting) {
        this.registration.waiting.postMessage({ type: 'SKIP_WAITING' });
      }

      this.setupUpdateListener();
      this.startPeriodicUpdates();
    } catch (error) {
      console.warn('Service Worker registration failed:', error);
    }
  }

  setupUpdateListener() {
    if (!this.registration) return;

    this.registration.addEventListener('updatefound', () => {
      const newWorker = this.registration.installing;
      
      newWorker.addEventListener('statechange', () => {
        if (newWorker.state === 'installed' && navigator.serviceWorker.controller) {
          newWorker.postMessage({ type: 'SKIP_WAITING' });
        }
      });
    });
  }

  startPeriodicUpdates() {
    if (!this.registration) return;

    setInterval(() => {
      this.registration.update().catch(err => {
        console.warn('Service Worker update failed:', err);
      });
    }, CONFIG.SERVICE_WORKER_UPDATE_INTERVAL);
  }
}

// ============================================================================
// Canvas Monitor - Detects when game starts rendering
// ============================================================================

class CanvasMonitor {
  constructor(canvas, uiManager) {
    this.canvas = canvas;
    this.uiManager = uiManager;
    this.checkInterval = null;
  }

  start() {
    if (this.checkInterval) return;
    
    setTimeout(() => {
      this.checkInterval = setInterval(() => {
        if (this.uiManager.hasEverLoaded) {
          this.stop();
          return;
        }

        if (this.isRendering()) {
          this.uiManager.forceHide();
          this.stop();
        }
      }, CONFIG.GAME_INIT_CHECK_INTERVAL);
    }, CONFIG.GAME_INIT_CHECK_DELAY);
  }

  isRendering() {
    try {
      if (!this.canvas || this.canvas.width === 0 || this.canvas.height === 0) {
        return false;
      }

      //
      // Try 2D context first

      const ctx2d = this.canvas.getContext('2d');
      if (ctx2d && this.hasNonBlackPixels2D(ctx2d)) {
        return true;
      }

      //
      // Try WebGL context

      const gl = this.canvas.getContext('webgl') || this.canvas.getContext('webgl2');
      if (gl && this.hasNonBlackPixelsGL(gl)) {
        return true;
      }

      return false;
    } catch (error) {
      // Silently handle errors (likely cross-origin or context issues)
      return false;
    }
  }

  hasNonBlackPixels2D(ctx) {
    const width = Math.min(this.canvas.width, 100);
    const height = Math.min(this.canvas.height, 100);
    const imageData = ctx.getImageData(0, 0, width, height);
    const data = imageData.data;

    for (let i = 0; i < data.length; i += 4) {
      const r = data[i];
      const g = data[i + 1];
      const b = data[i + 2];
      const a = data[i + 3];
      
      if (a > 0 && (r > 10 || g > 10 || b > 10)) {
        return true;
      }
    }

    return false;
  }

  hasNonBlackPixelsGL(gl) {
    const pixels = new Uint8Array(4);
    gl.readPixels(0, 0, 1, 1, gl.RGBA, gl.UNSIGNED_BYTE, pixels);
    return pixels.some(val => val > 0);
  }

  stop() {
    if (this.checkInterval) {
      clearInterval(this.checkInterval);
      this.checkInterval = null;
    }
  }
}

// ============================================================================
// XHR Interceptor for .data file tracking
// ============================================================================

class XHRInterceptor {
  constructor(assetLoader) {
    this.assetLoader = assetLoader;
    this.originalOpen = XMLHttpRequest.prototype.open;
    this.install();
  }

  install() {
    const self = this;
    
    XMLHttpRequest.prototype.open = function(method, url, ...args) {
      if (url.includes('.data')) {
        
        this.addEventListener('progress', (event) => {
          if (event.lengthComputable) {
            const percent = (event.loaded / event.total) * 100;
            self.assetLoader.onDataFileProgress(percent);
          }
        });
        
        this.addEventListener('load', () => {
          self.assetLoader.onDataFileLoad();
        });
        
        this.addEventListener('error', () => {
          self.assetLoader.onDataFileError();
        });
      }
      
      return self.originalOpen.call(this, method, url, ...args);
    };
  }

  uninstall() {
    XMLHttpRequest.prototype.open = this.originalOpen;
  }
}

// ============================================================================
// Timer Overrides
// ============================================================================

class TimerOverrides {
  constructor() {
    this.originalSetTimeout = window.setTimeout;
    this.originalClearTimeout = window.clearTimeout;
    this.originalRAF = window.requestAnimationFrame;
    this.originalCancelRAF = window.cancelAnimationFrame;
    this.rafIdCounter = CONFIG.RAF_ID_START;
  }

  install() {
    const self = this;

    window.setTimeout = function(callback, delay, ...args) {
      if (delay === 0 || delay === undefined || delay === null) {
        self.createImmediateCallback(callback, args);
        return Math.floor(Math.random() * CONFIG.IMMEDIATE_TIMER_ID_MIN);
      }
      return self.originalSetTimeout.call(window, callback, delay, ...args);
    };

    window.clearTimeout = function(id) {
      if (typeof id === 'number' && id < CONFIG.IMMEDIATE_TIMER_ID_MIN) {
        return self.originalClearTimeout.call(window, id);
      }
    };

    window.requestAnimationFrame = function(callback) {
      self.createImmediateCallback(callback, [performance.now()]);
      return self.rafIdCounter++;
    };

    window.cancelAnimationFrame = function(id) {
      // No op for immediate callbacks
    };
  }

  createImmediateCallback(callback, args = []) {
    if (window.MessageChannel) {
      const channel = new MessageChannel();
      channel.port2.onmessage = () => callback(...args);
      channel.port1.postMessage(null);
      return;
    }

    if (window.postMessage) {
      const messageKey = `immediate_callback_${Math.random()}`;
      const handler = (event) => {
        if (event.data === messageKey) {
          window.removeEventListener('message', handler);
          callback(...args);
        }
      };
      window.addEventListener('message', handler);
      window.postMessage(messageKey, '*');
      return;
    }

    callback(...args);
  }

  uninstall() {
    window.setTimeout = this.originalSetTimeout;
    window.clearTimeout = this.originalClearTimeout;
    window.requestAnimationFrame = this.originalRAF;
    window.cancelAnimationFrame = this.originalCancelRAF;
  }
}

// ============================================================================
// Main Application
// ============================================================================

// EDIT ME!
// The name of the application
class ApplicationApp {
  constructor() {
    this.canvas = document.getElementById('canvas');
    this.uiManager = new LoadingUIManager();
    this.fullscreenManager = new FullscreenManager(this.canvas);
    this.assetLoader = new AssetLoader(this.uiManager);
    this.serviceWorkerManager = new ServiceWorkerManager();
    this.canvasMonitor = new CanvasMonitor(this.canvas, this.uiManager);
    this.xhrInterceptor = new XHRInterceptor(this.assetLoader);
    this.timerOverrides = new TimerOverrides();
    
    this.init();
  }

  init() {
    this.timerOverrides.install();
    this.setupCanvasListeners();
    this.setupErrorHandlers();
    this.setupKeyboardShortcuts();
    this.serviceWorkerManager.register();
    this.canvasMonitor.start();
    this.setupModule();
    this.uiManager.startProgressMonitoring();
    
    // EDIT ME!
    // The name of the application
    console.log('Application initialized');
  }

  setupCanvasListeners() {
    this.canvas.addEventListener('webglcontextlost', (e) => {
      if (!this.uiManager.hasEverLoaded) {
        this.uiManager.showError('WebGL context lost. Please reload the page.');
      }
      e.preventDefault();
    }, false);

    this.canvas.addEventListener('mousedown', () => {
      this.canvas.focus();
    });

    window.addEventListener('load', () => {
      this.canvas.focus();
    });
  }

  setupErrorHandlers() {
    window.onerror = (event, source, lineno, colno, error) => {
      console.error('Global error:', { event, source, lineno, colno, error });
      if (!this.uiManager.hasEverLoaded) {
        this.uiManager.showError('INITIALIZATION ERROR - CHECK CONSOLE');
      }
      return true;
    };

    window.addEventListener('unhandledrejection', (event) => {
      console.error('Unhandled promise rejection:', event.reason);
      if (!this.uiManager.hasEverLoaded) {
        this.uiManager.showError('LOADING ERROR - PLEASE REFRESH');
      }
    });
  }

  setupKeyboardShortcuts() {
    document.addEventListener('keydown', (e) => {
      if (e.key === 'F9' && e.ctrlKey) {
        console.log('Force hiding loading screen via Ctrl+F9');
        this.uiManager.forceHide();
        this.canvasMonitor.stop();
      }
    });
  }

  setupModule() {
    const self = this;
    
    window.Module = window.Module || {};
    window.Module.canvas = this.canvas;
    
    Object.assign(window.Module, {
      setStatus(text) {
        if (text && !self.uiManager.hasEverLoaded && !self.uiManager.errorOccurred) {
          self.uiManager.setInfo(text.toUpperCase());
        }
      },

      monitorRunDependencies(left) {
        self.assetLoader.monitorRunDependencies(left);
      },

      requestFullscreen(lockPointer, resizeCanvas) {
        self.fullscreenManager.requestFullscreen();
      }
    });
  }

  destroy() {
    this.canvasMonitor.stop();
    this.fullscreenManager.destroy();
    this.xhrInterceptor.uninstall();
    this.timerOverrides.uninstall();
  }
}

// EDIT ME!
// The name of the application
const applicationApp = new ApplicationApp();

window.ApplicationApp = applicationApp;