/**
 * Error handling utilities for consistent error management across the application
 */

// Custom error classes
class ApiError extends Error {
    constructor(message, statusCode, details = null) {
      super(message);
      this.name = this.constructor.name;
      this.statusCode = statusCode;
      this.details = details;
      Error.captureStackTrace(this, this.constructor);
    }
  }
  
  // Common HTTP error subclasses
  class BadRequestError extends ApiError {
    constructor(message = 'Bad request', details = null) {
      super(message, 400, details);
    }
  }
  
  class UnauthorizedError extends ApiError {
    constructor(message = 'Unauthorized', details = null) {
      super(message, 401, details);
    }
  }
  
  class ForbiddenError extends ApiError {
    constructor(message = 'Forbidden', details = null) {
      super(message, 403, details);
    }
  }
  
  class NotFoundError extends ApiError {
    constructor(message = 'Resource not found', details = null) {
      super(message, 404, details);
    }
  }
  
  class ConflictError extends ApiError {
    constructor(message = 'Resource conflict', details = null) {
      super(message, 409, details);
    }
  }
  
  class ValidationError extends BadRequestError {
    constructor(message = 'Validation error', errors = {}) {
      super(message, errors);
    }
  }
  
  class DatabaseError extends ApiError {
    constructor(message = 'Database error', details = null) {
      super(message, 500, details);
    }
  }
  
  // Express middleware for handling errors
  const errorMiddleware = (err, req, res, next) => {
    console.error('Error details:', err);
    
    // Log client information for better debugging
    const clientIp = req.headers['x-forwarded-for'] || req.connection.remoteAddress;
    const userAgent = req.headers['user-agent'];
    const userId = req.user ? req.user.id : 'anonymous';
    
    console.error('Request context:', {
      path: req.path,
      method: req.method,
      clientIp,
      userAgent,
      userId
    });
    
    // Log full error stack trace in development
    if (process.env.NODE_ENV !== 'production') {
      console.error(err.stack);
    }
    
    // If it's our custom API error, use its status code and message
    if (err instanceof ApiError) {
      return res.status(err.statusCode).json({
        error: err.message,
        details: err.details
      });
    }
    
    // For validation errors from express-validator
    if (err.array && typeof err.array === 'function') {
      return res.status(400).json({
        error: 'Validation error',
        details: err.array()
      });
    }
    
    // Handle JWT errors
    if (err.name === 'JsonWebTokenError') {
      return res.status(401).json({ error: 'Invalid token' });
    }
    
    if (err.name === 'TokenExpiredError') {
      return res.status(401).json({ error: 'Token expired' });
    }
    
    // Default to 500 internal server error for unhandled exceptions
    res.status(500).json({
      error: 'Internal server error',
      details: process.env.NODE_ENV === 'production' ? null : err.message
    });
  };
  
  // Async handler to avoid try/catch boilerplate in route handlers
  const asyncHandler = (fn) => (req, res, next) => {
    Promise.resolve(fn(req, res, next)).catch(next);
  };
  
  // Helper for input validation
  const validateInput = (schema) => {
    return (req, res, next) => {
      const { error } = schema.validate(req.body);
      if (error) {
        throw new ValidationError('Validation error', error.details);
      }
      next();
    };
  };
  
  module.exports = {
    ApiError,
    BadRequestError,
    UnauthorizedError, 
    ForbiddenError,
    NotFoundError,
    ConflictError,
    ValidationError,
    DatabaseError,
    errorMiddleware,
    asyncHandler,
    validateInput
  };