#!/usr/bin/env node
/**
 * Athena Agent Main Server
 *
 * AI-powered agent for Athena Browser with Claude Agent SDK and MCP tools.
 * Listens on Unix socket for IPC with the GTK application.
 */

import express from 'express';
import { unlinkSync, existsSync, chmodSync, mkdirSync } from 'fs';
import { dirname } from 'path';
import { Logger } from './logger.js';
import { config, validateConfig } from './config.js';
import { ClaudeClient } from './claude-client.js';
import { createAthenaBrowserMcpServer, setBrowserApiBase } from './mcp-server.js';
import { healthHandler } from './routes/health.js';
import {
  createSendHandler,
  createContinueHandler,
  createClearHandler,
  capabilitiesHandler
} from './routes/chat.js';
import {
  navigateHandler,
  backHandler,
  forwardHandler,
  reloadHandler,
  getUrlHandler,
  getHtmlHandler,
  executeJsHandler,
  screenshotHandler,
  createTabHandler,
  closeTabHandler,
  switchTabHandler,
  tabInfoHandler,
  setBrowserController
} from './routes/browser.js';
import { createMockBrowserController } from './browser-controller-impl.js';
import { createNativeBrowserController } from './native-controller.js';
import { openUrlHandler } from './routes/poc.js';

const logger = new Logger('Server');

// ============================================================================
// Initialization
// ============================================================================

async function main() {
  try {
    logger.info('Starting Athena Agent', {
      version: '1.0.0',
      nodeVersion: process.version,
      platform: process.platform,
      pid: process.pid
    });

    // Validate configuration
    validateConfig(config);
    logger.info('Configuration validated', {
      socketPath: config.socketPath,
      cwd: config.cwd,
      model: config.model,
      permissionMode: config.permissionMode
    });

    // Configure MCP server to call back to this server's endpoints
    setBrowserApiBase(config.socketPath);
    logger.info('MCP browser API base configured', { socketPath: config.socketPath });

    // Register browser controller (try native first, fall back to mock)
    const nativeController = createNativeBrowserController();
    const browserController = nativeController || createMockBrowserController();
    setBrowserController(browserController);
    logger.info('Browser controller registered', {
      type: nativeController ? 'native' : 'mock'
    });

    // Create MCP server
    const mcpServer = createAthenaBrowserMcpServer();
    logger.info('MCP server created');

    // Create Claude client
    const claudeClient = new ClaudeClient(config, mcpServer);
    logger.info('Claude client created');

    // Create Express app
    const app = express();

    // Middleware
    app.use(express.json({ limit: '10mb' }));

    // Request logging middleware
    app.use((req, res, next) => {
      const requestId = req.headers['x-request-id'] as string || `req-${Date.now()}-${Math.random().toString(36).substr(2, 9)}`;
      (req as any).requestId = requestId;
      res.setHeader('X-Request-Id', requestId);

      const startTime = Date.now();
      res.on('finish', () => {
        const duration = Date.now() - startTime;
        logger.info('HTTP Request', {
          requestId,
          method: req.method,
          path: req.path,
          status: res.statusCode,
          duration
        });
      });

      next();
    });

    // ========================================================================
    // Routes
    // ========================================================================

    // Health check
    app.get('/health', healthHandler);

    // Capabilities
    app.get('/v1/capabilities', capabilitiesHandler);

    // Chat endpoints
    app.post('/v1/chat/send', createSendHandler(claudeClient));
    app.post('/v1/chat/continue', createContinueHandler(claudeClient));
    app.post('/v1/chat/clear', createClearHandler(claudeClient));

    // Browser control endpoints
    app.post('/v1/browser/navigate', navigateHandler);
    app.post('/v1/browser/back', backHandler);
    app.post('/v1/browser/forward', forwardHandler);
    app.post('/v1/browser/reload', reloadHandler);
    app.get('/v1/browser/get_url', getUrlHandler);
    app.get('/v1/browser/get_html', getHtmlHandler);
    app.post('/v1/browser/execute_js', executeJsHandler);
    app.post('/v1/browser/screenshot', screenshotHandler);

    // Tab management endpoints
    app.post('/v1/window/create_tab', createTabHandler);
    app.post('/v1/window/close_tab', closeTabHandler);
    app.post('/v1/window/switch_tab', switchTabHandler);
    app.get('/v1/window/tab_info', tabInfoHandler);

    // POC endpoint
    app.post('/v1/poc/open_url', openUrlHandler);

    // 404 handler
    app.use((req, res) => {
      res.status(404).json({
        error: 'Not Found',
        message: `Endpoint ${req.method} ${req.path} not found`,
        availableEndpoints: [
          'GET /health',
          'GET /v1/capabilities',
          'POST /v1/chat/send',
          'POST /v1/chat/continue',
          'POST /v1/chat/clear',
          'POST /v1/browser/navigate',
          'POST /v1/browser/back',
          'POST /v1/browser/forward',
          'POST /v1/browser/reload',
          'GET /v1/browser/get_url',
          'GET /v1/browser/get_html',
          'POST /v1/browser/execute_js',
          'POST /v1/browser/screenshot',
          'POST /v1/window/create_tab',
          'POST /v1/window/close_tab',
          'POST /v1/window/switch_tab',
          'GET /v1/window/tab_info'
        ]
      });
    });

    // Error handler
    app.use((err: any, req: express.Request, res: express.Response, _next: express.NextFunction) => {
      logger.error('Unhandled error', {
        requestId: (req as any).requestId,
        error: err.message,
        stack: err.stack
      });

      res.status(500).json({
        error: 'Internal Server Error',
        message: err.message,
        requestId: (req as any).requestId
      });
    });

    // ========================================================================
    // Server Startup
    // ========================================================================

    await startServer(app);

  } catch (error) {
    logger.error('Failed to start server', {
      error: error instanceof Error ? error.message : 'Unknown error',
      stack: error instanceof Error ? error.stack : undefined
    });
    process.exit(1);
  }
}

/**
 * Start the server on Unix socket
 */
function startServer(app: express.Application): Promise<void> {
  return new Promise((resolve, reject) => {
    // Remove existing socket if present
    if (existsSync(config.socketPath)) {
      logger.warn('Removing existing socket file', { path: config.socketPath });
      try {
        unlinkSync(config.socketPath);
      } catch (error) {
        logger.error('Failed to remove socket', {
          error: error instanceof Error ? error.message : 'Unknown error'
        });
      }
    }

    // Create socket directory if needed
    const socketDir = dirname(config.socketPath);
    if (!existsSync(socketDir)) {
      mkdirSync(socketDir, { recursive: true, mode: 0o700 });
    }

    // Start listening on Unix socket
    const server = app.listen(config.socketPath, () => {
      // Set socket permissions to user-only (0600)
      try {
        chmodSync(config.socketPath, 0o600);
      } catch (error) {
        logger.warn('Failed to set socket permissions', {
          error: error instanceof Error ? error.message : 'Unknown error'
        });
      }

      logger.info('Server ready', {
        socket: config.socketPath,
        version: '1.0.0',
        pid: process.pid
      });

      // Print READY line for C++ to consume
      console.log(`READY ${config.socketPath}`);

      resolve();
    });

    server.on('error', (err) => {
      logger.error('Server error', { error: err.message });
      reject(err);
    });

    // Graceful shutdown
    const gracefulShutdown = (signal: string) => {
      logger.info('Received shutdown signal', { signal });

      server.close(() => {
        logger.info('Server closed');

        // Clean up socket file
        if (existsSync(config.socketPath)) {
          try {
            unlinkSync(config.socketPath);
            logger.debug('Socket file removed', { path: config.socketPath });
          } catch (error) {
            logger.warn('Failed to remove socket', {
              error: error instanceof Error ? error.message : 'Unknown error'
            });
          }
        }

        logger.info('Graceful shutdown complete');
        process.exit(0);
      });

      // Force exit after grace period
      setTimeout(() => {
        logger.warn('Forced shutdown after timeout');
        process.exit(1);
      }, 2000);
    };

    process.on('SIGTERM', () => gracefulShutdown('SIGTERM'));
    process.on('SIGINT', () => gracefulShutdown('SIGINT'));

    process.on('uncaughtException', (err) => {
      logger.error('Uncaught exception', {
        error: err.message,
        stack: err.stack
      });
      process.exit(1);
    });

    process.on('unhandledRejection', (reason) => {
      logger.error('Unhandled rejection', {
        reason: reason instanceof Error ? reason.message : String(reason),
        stack: reason instanceof Error ? reason.stack : undefined
      });
    });
  });
}

// ============================================================================
// Start the server
// ============================================================================

main().catch((error) => {
  console.error('Fatal error:', error);
  process.exit(1);
});
