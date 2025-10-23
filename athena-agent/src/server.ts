#!/usr/bin/env node
/**
 * Athena Agent Main Server
 *
 * AI-powered agent for Athena Browser with Claude Agent SDK and MCP tools.
 * Listens on Unix socket for IPC with the GTK application.
 */

import express from 'express';
import { unlinkSync, existsSync, mkdirSync } from 'fs';
import { dirname } from 'path';
import { Logger } from './logger';
import { config, validateConfig } from './config';
import { ClaudeClient } from './claude-client';
import { SessionManager } from './session-manager';
import { healthHandler } from './routes/health';
import {
  createSendHandler,
  createContinueHandler,
  createClearHandler,
  createStreamHandler,
  capabilitiesHandler
} from './routes/chat';
import {
  createListHandler,
  createGetHandler,
  createGetMessagesHandler,
  createUpdateHandler,
  createDeleteHandler,
  createSearchHandler,
  createPruneHandler,
  createStatsHandler
} from './routes/sessions';
import { setBrowserController } from './routes/browser';
import { createMockBrowserController } from './browser-controller-impl';
import { createNativeBrowserController } from './native-controller';
import { openUrlHandler } from './routes/poc';
import { createV1Router } from './api/v1';
import { createAgentMcpServer } from './mcp-agent-adapter';

const logger = new Logger('Server');

// ============================================================================
// Initialization
// ============================================================================

async function main() {
  try {
    // Direct stderr write to test visibility
    process.stderr.write('[ATHENA-AGENT] Starting up...\n');

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

    // Detect browser control socket
    const uid = process.getuid?.() ?? 1000;
    const controlSocketPath = process.env.ATHENA_CONTROL_SOCKET_PATH || `/tmp/athena-${uid}-control.sock`;
    logger.info('Browser control socket detected', { controlSocketPath });

    // Register browser controller (try native first, fall back to mock)
    const nativeController = createNativeBrowserController();
    const browserController = nativeController || createMockBrowserController();
    setBrowserController(browserController);
    logger.info('Browser controller registered', {
      type: nativeController ? 'native' : 'mock'
    });

    // Create session manager for persistent conversation storage
    const sessionManager = new SessionManager();
    logger.info('Session manager initialized', {
      sessionCount: sessionManager.getSessionCount()
    });

    // Create MCP server for Claude Agent SDK integration
    // This allows Claude to use browser control tools via the Agent SDK
    const mcpServer = createAgentMcpServer(controlSocketPath);
    logger.info('MCP server created for Agent SDK');

    // Create Claude client with MCP server and session manager
    const claudeClient = new ClaudeClient(config, mcpServer, sessionManager);
    logger.info('Claude client created with MCP server and session storage');

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
    app.post('/v1/chat/stream', createStreamHandler(claudeClient));
    app.post('/v1/chat/continue', createContinueHandler(claudeClient));
    app.post('/v1/chat/clear', createClearHandler(claudeClient));

    // Session management endpoints
    app.get('/v1/sessions', createListHandler(sessionManager));
    app.get('/v1/sessions/search', createSearchHandler(sessionManager));
    app.get('/v1/sessions/stats', createStatsHandler(sessionManager));
    app.post('/v1/sessions/prune', createPruneHandler(sessionManager));
    app.get('/v1/sessions/:sessionId', createGetHandler(sessionManager));
    app.get('/v1/sessions/:sessionId/messages', createGetMessagesHandler(sessionManager));
    app.patch('/v1/sessions/:sessionId', createUpdateHandler(sessionManager));
    app.delete('/v1/sessions/:sessionId', createDeleteHandler(sessionManager));

    // Unified API (preferred entry point for MCP and other clients)
    app.use('/v1', createV1Router(browserController));

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
          'POST /v1/chat/stream',
          'POST /v1/chat/continue',
          'POST /v1/chat/clear',
          'GET /v1/sessions',
          'GET /v1/sessions/search',
          'GET /v1/sessions/stats',
          'POST /v1/sessions/prune',
          'GET /v1/sessions/:sessionId',
          'GET /v1/sessions/:sessionId/messages',
          'PATCH /v1/sessions/:sessionId',
          'DELETE /v1/sessions/:sessionId',
          'POST /v1/browser/navigate',
          'POST /v1/browser/back',
          'POST /v1/browser/forward',
          'POST /v1/browser/reload',
          'GET /v1/browser/url',
          'GET /v1/browser/html',
          'POST /v1/browser/execute-js',
          'POST /v1/browser/screenshot',
          'POST /v1/window/create',
          'POST /v1/window/close',
          'POST /v1/window/switch',
          'GET /v1/window/tabs',
          'POST /v1/poc/open_url'
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

    // Set umask to ensure socket is created with secure permissions (0600)
    // This prevents a race condition where the socket could be accessed
    // before chmodSync() is called
    const oldMask = process.umask(0o077);  // 0o077 → files created with 0o600

    // Start listening on Unix socket
    const server = app.listen(config.socketPath, () => {
      // Restore original umask
      process.umask(oldMask);

      logger.info('Server ready', {
        socket: config.socketPath,
        version: '1.0.0',
        pid: process.pid,
        socketPermissions: '0600'
      });

      // Print READY line for C++ to consume
      // Only print to stdout if we're NOT running as an MCP stdio server
      // (MCP stdio protocol requires stdout to only contain JSON-RPC messages)
      const isMcpStdio = process.env.MCP_STDIO === 'true' || process.stdin.isTTY === false;
      if (!isMcpStdio) {
        console.log(`READY ${config.socketPath}`);
      }

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
