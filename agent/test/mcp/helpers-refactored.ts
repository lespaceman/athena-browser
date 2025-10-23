/**
 * MCP Testing Helpers (Refactored)
 *
 * Utility functions for testing the refactored MCP server implementation.
 */

import { Client } from '@modelcontextprotocol/sdk/client/index.js';
import { StdioClientTransport } from '@modelcontextprotocol/sdk/client/stdio.js';
import { CallToolResult } from '@modelcontextprotocol/sdk/types.js';

/**
 * Create an MCP client connected to the refactored athena-agent server
 */
export async function createMcpClient(socketPath: string): Promise<Client> {
  const client = new Client(
    {
      name: 'athena-mcp-test-client',
      version: '2.0.0'
    },
    {
      capabilities: {
        sampling: {} // Enable sampling for testing
      }
    }
  );

  const transport = new StdioClientTransport({
    command: 'node',
    args: ['dist/mcp-stdio-server-refactored.js'],
    env: {
      ...process.env,
      ATHENA_SOCKET_PATH: socketPath
    }
  });

  await client.connect(transport);
  return client;
}

/**
 * Extract text content from a tool result
 */
export function extractTextContent(result: CallToolResult): string {
  const textContent = result.content.find(c => c.type === 'text');
  if (textContent && textContent.type === 'text') {
    return textContent.text;
  }
  return '';
}

/**
 * Extract structured content from a tool result
 */
export function extractStructuredContent(result: CallToolResult): any {
  return (result as any).structuredContent;
}

/**
 * Extract image data from a tool result
 */
export function extractImageData(result: CallToolResult): string | null {
  const imageContent = result.content.find(c => c.type === 'image');
  if (imageContent && imageContent.type === 'image') {
    return imageContent.data;
  }
  return null;
}

/**
 * Check if a tool result contains an error
 */
export function isErrorResult(result: CallToolResult): boolean {
  return result.isError === true;
}

/**
 * Wait for a condition to be true
 */
export async function waitFor(
  condition: () => boolean | Promise<boolean>,
  timeout: number = 5000,
  interval: number = 100
): Promise<void> {
  const startTime = Date.now();
  while (Date.now() - startTime < timeout) {
    if (await condition()) {
      return;
    }
    await new Promise(resolve => setTimeout(resolve, interval));
  }
  throw new Error(`Timeout waiting for condition after ${timeout}ms`);
}

/**
 * Validate that a tool has the expected schema properties
 */
export function validateToolSchema(
  tool: any,
  expectedName: string,
  expectedInputProperties: string[],
  expectedOutputProperties: string[]
): void {
  if (tool.name !== expectedName) {
    throw new Error(`Expected tool name '${expectedName}', got '${tool.name}'`);
  }

  if (!tool.inputSchema || !tool.inputSchema.properties) {
    throw new Error(`Tool '${expectedName}' missing inputSchema.properties`);
  }

  for (const prop of expectedInputProperties) {
    if (!(prop in tool.inputSchema.properties)) {
      throw new Error(`Tool '${expectedName}' missing input property '${prop}'`);
    }
  }

  // In the refactored version, output schema is part of the tool definition
  // but not exposed in the same way - we'll validate through structured content instead
}

/**
 * Common test data for tool calls
 */
export const TEST_DATA = {
  validUrl: 'https://example.com',
  invalidUrl: 'not-a-valid-url',
  jsCode: 'document.title',
  queryTypes: ['forms', 'navigation', 'article', 'tables', 'media'] as const
};

/**
 * Expected tool names (for validation)
 * Note: Refactored version has fewer tools (removed some context-efficient ones for core focus)
 */
export const EXPECTED_TOOLS = {
  navigation: [
    'browser_navigate',
    'browser_back',
    'browser_forward',
    'browser_reload'
  ],
  information: [
    'browser_get_url',
    'browser_get_html',
    'browser_get_page_summary'
  ],
  interaction: [
    'browser_execute_js',
    'browser_screenshot'
  ],
  tabManagement: [
    'window_create_tab',
    'window_close_tab',
    'window_switch_tab'
  ]
};

/**
 * Get all expected tool names as a flat array
 */
export function getAllExpectedTools(): string[] {
  return [
    ...EXPECTED_TOOLS.navigation,
    ...EXPECTED_TOOLS.information,
    ...EXPECTED_TOOLS.interaction,
    ...EXPECTED_TOOLS.tabManagement
  ];
}
