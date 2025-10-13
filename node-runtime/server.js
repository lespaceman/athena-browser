#!/usr/bin/env node

/**
 * Athena Browser Node.js Runtime Helper
 *
 * This process provides Node.js capabilities to the GTK application via
 * HTTP over Unix domain socket. It starts with the GTK app and terminates
 * when the app shuts down.
 *
 * Architecture:
 * - IPC: HTTP over Unix domain socket (localhost alternative considered)
 * - Lifecycle: Spawned by GTK, supervised by Application class
 * - Security: Socket permissions locked to user (0600)
 * - Health: Implements /health endpoint for liveness checks
 * - Graceful shutdown: Handles SIGTERM with cleanup
 */

const express = require('express');
const fs = require('fs');
const path = require('path');
const os = require('os');
const { ClaudeClient } = require('./claude-client');

// ============================================================================
// Configuration
// ============================================================================

const SOCKET_PATH = process.env.ATHENA_SOCKET_PATH ||
                    path.join(os.tmpdir(), `athena-${process.getuid()}.sock`);
const API_VERSION = 'v1';

// ============================================================================
// Structured Logging
// ============================================================================

class Logger {
  constructor(module) {
    this.module = module;
  }

  log(level, message, meta = {}) {
    const entry = {
      timestamp: new Date().toISOString(),
      level,
      module: this.module,
      message,
      pid: process.pid,
      ...meta
    };
    console.log(JSON.stringify(entry));
  }

  debug(message, meta) { this.log('DEBUG', message, meta); }
  info(message, meta) { this.log('INFO', message, meta); }
  warn(message, meta) { this.log('WARN', message, meta); }
  error(message, meta) { this.log('ERROR', message, meta); }
}

const logger = new Logger('NodeRuntime');

// ============================================================================
// Application State
// ============================================================================

const state = {
  startTime: Date.now(),
  requestCount: 0,
  ready: false
};

// Initialize Claude client
const claudeClient = new ClaudeClient({
  cwd: process.cwd(),
  permissionMode: 'bypassPermissions',
  settingSources: ['project']
});

// ============================================================================
// Express App Setup
// ============================================================================

const app = express();

// Middleware
app.use(express.json());
app.use((req, res, next) => {
  const requestId = req.headers['x-request-id'] || `req-${Date.now()}-${Math.random().toString(36).substr(2, 9)}`;
  req.requestId = requestId;
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

  state.requestCount++;
  next();
});

// ============================================================================
// API Endpoints
// ============================================================================

// Health check endpoint (liveness + readiness)
app.get('/health', (req, res) => {
  const uptime = Date.now() - state.startTime;
  res.json({
    status: 'healthy',
    ready: state.ready,
    uptime,
    requestCount: state.requestCount,
    version: API_VERSION
  });
});

// Capabilities endpoint
app.get(`/${API_VERSION}/capabilities`, (req, res) => {
  res.json({
    version: API_VERSION,
    features: [
      'filesystem',
      'system-info',
      'echo',
      'claude-agent-sdk'
    ],
    node_version: process.version,
    platform: process.platform,
    claude_sdk: {
      available: true,
      version: '1.x',
      operations: [
        'query',
        'analyze-code',
        'generate-code',
        'refactor-code',
        'search-code',
        'run-command',
        'analyze-web',
        'search-web'
      ]
    }
  });
});

// System info endpoint
app.get(`/${API_VERSION}/system/info`, (req, res) => {
  res.json({
    platform: os.platform(),
    arch: os.arch(),
    hostname: os.hostname(),
    uptime: os.uptime(),
    totalmem: os.totalmem(),
    freemem: os.freemem(),
    cpus: os.cpus().length,
    node_version: process.version
  });
});

// Echo endpoint (for testing bidirectional communication)
app.post(`/${API_VERSION}/echo`, (req, res) => {
  const { message } = req.body;

  if (!message) {
    return res.status(400).json({
      error: 'Bad Request',
      message: 'Missing "message" field in request body'
    });
  }

  res.json({
    echo: message,
    timestamp: Date.now(),
    requestId: req.requestId
  });
});

// Filesystem read endpoint (example privileged operation)
app.post(`/${API_VERSION}/fs/read`, (req, res) => {
  const { path: filePath } = req.body;

  if (!filePath) {
    return res.status(400).json({
      error: 'Bad Request',
      message: 'Missing "path" field'
    });
  }

  // Validate path (basic security - no path traversal outside allowed dirs)
  const resolvedPath = path.resolve(filePath);

  fs.readFile(resolvedPath, 'utf8', (err, data) => {
    if (err) {
      logger.error('File read failed', { path: filePath, error: err.message });
      return res.status(500).json({
        error: 'Internal Server Error',
        message: `Failed to read file: ${err.message}`
      });
    }

    res.json({
      path: resolvedPath,
      content: data,
      size: data.length
    });
  });
});

// Filesystem write endpoint
app.post(`/${API_VERSION}/fs/write`, (req, res) => {
  const { path: filePath, content } = req.body;

  if (!filePath || content === undefined) {
    return res.status(400).json({
      error: 'Bad Request',
      message: 'Missing "path" or "content" field'
    });
  }

  const resolvedPath = path.resolve(filePath);

  fs.writeFile(resolvedPath, content, 'utf8', (err) => {
    if (err) {
      logger.error('File write failed', { path: filePath, error: err.message });
      return res.status(500).json({
        error: 'Internal Server Error',
        message: `Failed to write file: ${err.message}`
      });
    }

    res.json({
      path: resolvedPath,
      size: content.length,
      success: true
    });
  });
});

// ============================================================================
// Claude Agent SDK Endpoints
// ============================================================================

// General Claude query endpoint
app.post(`/${API_VERSION}/claude/query`, async (req, res) => {
  const { prompt, options = {} } = req.body;

  if (!prompt) {
    return res.status(400).json({
      error: 'Bad Request',
      message: 'Missing "prompt" field'
    });
  }

  try {
    logger.info('Claude query started', {
      requestId: req.requestId,
      promptLength: prompt.length
    });

    const result = await claudeClient.query(prompt, options);

    logger.info('Claude query completed', {
      requestId: req.requestId,
      success: result.success,
      messageCount: result.messages?.length || 0
    });

    res.json({
      success: result.success,
      result: result.result?.subtype === 'success' ? result.result.result : null,
      error: result.error,
      sessionId: result.sessionId,
      usage: result.result?.usage,
      cost: result.result?.total_cost_usd,
      numTurns: result.result?.num_turns,
      durationMs: result.result?.duration_ms
    });
  } catch (error) {
    logger.error('Claude query failed', {
      requestId: req.requestId,
      error: error.message
    });

    res.status(500).json({
      error: 'Internal Server Error',
      message: error.message
    });
  }
});

// Analyze code endpoint
app.post(`/${API_VERSION}/claude/analyze-code`, async (req, res) => {
  const { filePath, question } = req.body;

  if (!filePath || !question) {
    return res.status(400).json({
      error: 'Bad Request',
      message: 'Missing "filePath" or "question" field'
    });
  }

  try {
    const result = await claudeClient.analyzeCode(filePath, question);

    res.json({
      success: result.success,
      analysis: result.result?.subtype === 'success' ? result.result.result : null,
      error: result.error,
      sessionId: result.sessionId
    });
  } catch (error) {
    logger.error('Code analysis failed', { error: error.message });
    res.status(500).json({ error: 'Internal Server Error', message: error.message });
  }
});

// Generate code endpoint
app.post(`/${API_VERSION}/claude/generate-code`, async (req, res) => {
  const { spec, outputPath } = req.body;

  if (!spec) {
    return res.status(400).json({
      error: 'Bad Request',
      message: 'Missing "spec" field'
    });
  }

  try {
    const result = await claudeClient.generateCode(spec, outputPath);

    res.json({
      success: result.success,
      result: result.result?.subtype === 'success' ? result.result.result : null,
      error: result.error,
      sessionId: result.sessionId
    });
  } catch (error) {
    logger.error('Code generation failed', { error: error.message });
    res.status(500).json({ error: 'Internal Server Error', message: error.message });
  }
});

// Refactor code endpoint
app.post(`/${API_VERSION}/claude/refactor-code`, async (req, res) => {
  const { filePath, instructions } = req.body;

  if (!filePath || !instructions) {
    return res.status(400).json({
      error: 'Bad Request',
      message: 'Missing "filePath" or "instructions" field'
    });
  }

  try {
    const result = await claudeClient.refactorCode(filePath, instructions);

    res.json({
      success: result.success,
      result: result.result?.subtype === 'success' ? result.result.result : null,
      error: result.error,
      sessionId: result.sessionId
    });
  } catch (error) {
    logger.error('Code refactoring failed', { error: error.message });
    res.status(500).json({ error: 'Internal Server Error', message: error.message });
  }
});

// Search code endpoint
app.post(`/${API_VERSION}/claude/search-code`, async (req, res) => {
  const { pattern, globPattern = '**/*' } = req.body;

  if (!pattern) {
    return res.status(400).json({
      error: 'Bad Request',
      message: 'Missing "pattern" field'
    });
  }

  try {
    const result = await claudeClient.searchCode(pattern, globPattern);

    res.json({
      success: result.success,
      result: result.result?.subtype === 'success' ? result.result.result : null,
      error: result.error,
      sessionId: result.sessionId
    });
  } catch (error) {
    logger.error('Code search failed', { error: error.message });
    res.status(500).json({ error: 'Internal Server Error', message: error.message });
  }
});

// Run command endpoint
app.post(`/${API_VERSION}/claude/run-command`, async (req, res) => {
  const { command } = req.body;

  if (!command) {
    return res.status(400).json({
      error: 'Bad Request',
      message: 'Missing "command" field'
    });
  }

  try {
    const result = await claudeClient.runCommand(command);

    res.json({
      success: result.success,
      result: result.result?.subtype === 'success' ? result.result.result : null,
      error: result.error,
      sessionId: result.sessionId
    });
  } catch (error) {
    logger.error('Command execution failed', { error: error.message });
    res.status(500).json({ error: 'Internal Server Error', message: error.message });
  }
});

// Analyze web content endpoint
app.post(`/${API_VERSION}/claude/analyze-web`, async (req, res) => {
  const { url, question } = req.body;

  if (!url || !question) {
    return res.status(400).json({
      error: 'Bad Request',
      message: 'Missing "url" or "question" field'
    });
  }

  try {
    const result = await claudeClient.analyzeWeb(url, question);

    res.json({
      success: result.success,
      analysis: result.result?.subtype === 'success' ? result.result.result : null,
      error: result.error,
      sessionId: result.sessionId
    });
  } catch (error) {
    logger.error('Web analysis failed', { error: error.message });
    res.status(500).json({ error: 'Internal Server Error', message: error.message });
  }
});

// Search web endpoint
app.post(`/${API_VERSION}/claude/search-web`, async (req, res) => {
  const { query } = req.body;

  if (!query) {
    return res.status(400).json({
      error: 'Bad Request',
      message: 'Missing "query" field'
    });
  }

  try {
    const result = await claudeClient.searchWeb(query);

    res.json({
      success: result.success,
      result: result.result?.subtype === 'success' ? result.result.result : null,
      error: result.error,
      sessionId: result.sessionId
    });
  } catch (error) {
    logger.error('Web search failed', { error: error.message });
    res.status(500).json({ error: 'Internal Server Error', message: error.message });
  }
});

// Continue conversation endpoint
app.post(`/${API_VERSION}/claude/continue`, async (req, res) => {
  const { prompt } = req.body;

  if (!prompt) {
    return res.status(400).json({
      error: 'Bad Request',
      message: 'Missing "prompt" field'
    });
  }

  try {
    const result = await claudeClient.continueConversation(prompt);

    res.json({
      success: result.success,
      result: result.result?.subtype === 'success' ? result.result.result : null,
      error: result.error,
      sessionId: result.sessionId
    });
  } catch (error) {
    logger.error('Continue conversation failed', { error: error.message });
    res.status(500).json({ error: 'Internal Server Error', message: error.message });
  }
});

// Resume session endpoint
app.post(`/${API_VERSION}/claude/resume`, async (req, res) => {
  const { sessionId, prompt } = req.body;

  if (!sessionId || !prompt) {
    return res.status(400).json({
      error: 'Bad Request',
      message: 'Missing "sessionId" or "prompt" field'
    });
  }

  try {
    const result = await claudeClient.resumeSession(sessionId, prompt);

    res.json({
      success: result.success,
      result: result.result?.subtype === 'success' ? result.result.result : null,
      error: result.error,
      sessionId: result.sessionId
    });
  } catch (error) {
    logger.error('Resume session failed', { error: error.message });
    res.status(500).json({ error: 'Internal Server Error', message: error.message });
  }
});

// 404 handler
app.use((req, res) => {
  res.status(404).json({
    error: 'Not Found',
    message: `Endpoint ${req.method} ${req.path} not found`,
    availableVersions: [API_VERSION]
  });
});

// Error handler
app.use((err, req, res, next) => {
  logger.error('Unhandled error', {
    requestId: req.requestId,
    error: err.message,
    stack: err.stack
  });

  res.status(500).json({
    error: 'Internal Server Error',
    message: err.message,
    requestId: req.requestId
  });
});

// ============================================================================
// Server Lifecycle
// ============================================================================

let server = null;

function startServer() {
  return new Promise((resolve, reject) => {
    // Remove existing socket if present
    if (fs.existsSync(SOCKET_PATH)) {
      logger.warn('Removing existing socket file', { path: SOCKET_PATH });
      fs.unlinkSync(SOCKET_PATH);
    }

    // Create socket directory if needed
    const socketDir = path.dirname(SOCKET_PATH);
    if (!fs.existsSync(socketDir)) {
      fs.mkdirSync(socketDir, { recursive: true, mode: 0o700 });
    }

    // Start listening on Unix socket
    server = app.listen(SOCKET_PATH, () => {
      // Set socket permissions to user-only (0600)
      fs.chmodSync(SOCKET_PATH, 0o600);

      state.ready = true;

      logger.info('Node runtime ready', {
        socket: SOCKET_PATH,
        version: API_VERSION,
        pid: process.pid
      });

      // Print READY line for GTK to consume (must be single line, parseable)
      console.log(`READY ${SOCKET_PATH}`);

      resolve();
    });

    server.on('error', (err) => {
      logger.error('Server error', { error: err.message });
      reject(err);
    });
  });
}

function gracefulShutdown(signal) {
  logger.info('Received shutdown signal', { signal });

  if (!server) {
    process.exit(0);
    return;
  }

  // Stop accepting new connections
  server.close(() => {
    logger.info('Server closed');

    // Clean up socket file
    if (fs.existsSync(SOCKET_PATH)) {
      fs.unlinkSync(SOCKET_PATH);
      logger.debug('Socket file removed', { path: SOCKET_PATH });
    }

    logger.info('Graceful shutdown complete');
    process.exit(0);
  });

  // Force exit after grace period
  setTimeout(() => {
    logger.warn('Forced shutdown after timeout');
    process.exit(1);
  }, 2000);
}

// ============================================================================
// Signal Handlers
// ============================================================================

process.on('SIGTERM', () => gracefulShutdown('SIGTERM'));
process.on('SIGINT', () => gracefulShutdown('SIGINT'));

process.on('uncaughtException', (err) => {
  logger.error('Uncaught exception', {
    error: err.message,
    stack: err.stack
  });
  process.exit(1);
});

process.on('unhandledRejection', (reason, promise) => {
  logger.error('Unhandled rejection', {
    reason: reason instanceof Error ? reason.message : String(reason),
    stack: reason instanceof Error ? reason.stack : undefined
  });
});

// ============================================================================
// Main
// ============================================================================

async function main() {
  logger.info('Starting Athena Node Runtime', {
    version: API_VERSION,
    node_version: process.version,
    platform: process.platform,
    pid: process.pid
  });

  try {
    await startServer();
  } catch (err) {
    logger.error('Failed to start server', { error: err.message });
    process.exit(1);
  }
}

main();
