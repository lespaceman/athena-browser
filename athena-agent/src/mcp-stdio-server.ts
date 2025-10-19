#!/usr/bin/env node
/**
 * MCP Stdio Server Entry Point
 *
 * This is the stdio entry point for running the MCP server.
 * Used by MCP Inspector and Claude Desktop.
 *
 * The main server.ts runs as an HTTP server over Unix socket.
 * This wrapper creates an MCP server that can communicate via stdio.
 *
 * Following official MCP SDK patterns:
 * - Uses StdioServerTransport from @modelcontextprotocol/sdk
 * - Clean separation between transport and server logic
 * - Proper error handling and graceful shutdown
 */

import { StdioServerTransport } from '@modelcontextprotocol/sdk/server/stdio.js';
import { Logger } from './logger.js';
import { createAthenaBrowserMcpServer, setBrowserApiBase } from './mcp-server.js';

const logger = new Logger('MCP-Stdio');

async function main() {
  try {
    logger.info('Starting MCP stdio server');

    // Configure the MCP server to connect to the HTTP API
    // In stdio mode, we need to connect to an existing athena-agent server
    const uid = process.getuid?.() ?? 1000;
    const socketPath = process.env.ATHENA_SOCKET_PATH || `/tmp/athena-${uid}.sock`;

    setBrowserApiBase(socketPath);
    logger.info('Configured to connect to athena-agent', { socketPath });

    // Create the MCP server
    const mcpServer = createAthenaBrowserMcpServer();
    logger.info('MCP server created');

    // Connect via stdio transport
    const transport = new StdioServerTransport();
    await mcpServer.connect(transport);

    logger.info('MCP stdio server connected and ready');

    // The server will now communicate via stdin/stdout
    // All logging goes to stderr to avoid interfering with JSON-RPC messages

  } catch (error) {
    logger.error('Failed to start MCP stdio server', {
      error: error instanceof Error ? error.message : 'Unknown error',
      stack: error instanceof Error ? error.stack : undefined
    });
    process.exit(1);
  }
}

// Handle graceful shutdown
process.on('SIGTERM', () => {
  logger.info('Received SIGTERM, shutting down');
  process.exit(0);
});

process.on('SIGINT', () => {
  logger.info('Received SIGINT, shutting down');
  process.exit(0);
});

// Start the server
main().catch((error) => {
  console.error('Fatal error:', error);
  process.exit(1);
});
