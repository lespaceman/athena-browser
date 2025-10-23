/**
 * MCP Server Integration Tests
 *
 * Tests the Athena Browser MCP server using the official MCP Client SDK.
 * These tests validate that:
 * 1. The MCP server properly exposes all browser control tools
 * 2. Tools can be discovered via the MCP protocol
 * 3. Tool calls execute correctly and return proper responses
 * 4. Error handling works as expected
 *
 * Running these tests:
 *   npm run test:mcp
 *
 * Testing with MCP Inspector:
 *   npx @modelcontextprotocol/inspector node dist/server.js
 */

import { describe, it, expect, beforeAll, afterAll } from 'vitest';
import { Client } from '@modelcontextprotocol/sdk/client/index.js';
import { StdioClientTransport } from '@modelcontextprotocol/sdk/client/stdio.js';
import { unlinkSync, existsSync } from 'fs';
import { spawn, ChildProcess } from 'child_process';

const TEST_SOCKET = '/tmp/athena-test-mcp.sock';
const TEST_CONTROL_SOCKET = '/tmp/athena-test-mcp-control.sock';

describe('MCP Server - Athena Browser Tools', () => {
  let client: Client;
  let serverProcess: ChildProcess;

  beforeAll(async () => {
    // Clean up any existing sockets
    [TEST_SOCKET, TEST_CONTROL_SOCKET].forEach(sock => {
      if (existsSync(sock)) {
        unlinkSync(sock);
      }
    });

    // Start the athena-agent server process
    serverProcess = spawn('node', ['dist/server.js'], {
      env: {
        ...process.env,
        ATHENA_SOCKET_PATH: TEST_SOCKET,
        ATHENA_CONTROL_SOCKET_PATH: TEST_CONTROL_SOCKET
      },
      stdio: ['pipe', 'pipe', 'pipe']
    });

    // Wait for server to be ready
    await new Promise<void>((resolve, reject) => {
      const timeout = setTimeout(() => {
        reject(new Error('Server startup timeout'));
      }, 10000);

      serverProcess.stdout?.on('data', (data: Buffer) => {
        if (data.toString().includes('READY')) {
          clearTimeout(timeout);
          resolve();
        }
      });

      serverProcess.stderr?.on('data', (data: Buffer) => {
        // Log errors but don't fail - some are expected (e.g., mock controller warnings)
        const msg = data.toString();
        if (!msg.includes('Using mock') && !msg.includes('Browser controller')) {
          console.error('Server stderr:', msg);
        }
      });

      serverProcess.on('error', (err: Error) => {
        clearTimeout(timeout);
        reject(err);
      });
    });

    // Give server a moment to fully initialize
    await new Promise(resolve => setTimeout(resolve, 500));

    // Create MCP client and connect via stdio to a new server instance
    // This simulates how Claude Desktop would connect
    client = new Client({
      name: 'athena-mcp-test-client',
      version: '1.0.0'
    }, {
      capabilities: {
        sampling: {} // Enable sampling capability for testing
      }
    });

    const transport = new StdioClientTransport({
      command: 'node',
      args: ['dist/server.js'],
      env: {
        ...process.env,
        ATHENA_SOCKET_PATH: TEST_SOCKET,
        ATHENA_CONTROL_SOCKET_PATH: TEST_CONTROL_SOCKET
      }
    });

    await client.connect(transport);
  }, 15000);

  afterAll(async () => {
    if (client) {
      await client.close();
    }
    if (serverProcess) {
      serverProcess.kill();
      // Wait for process to exit
      await new Promise(resolve => setTimeout(resolve, 500));
    }
    // Clean up sockets
    [TEST_SOCKET, TEST_CONTROL_SOCKET].forEach(sock => {
      if (existsSync(sock)) {
        unlinkSync(sock);
      }
    });
  });

  describe('Tool Discovery', () => {
    it('should list all available tools', async () => {
      const response = await client.listTools();

      expect(response.tools).toBeDefined();
      expect(Array.isArray(response.tools)).toBe(true);
      expect(response.tools.length).toBeGreaterThan(0);
    });

    it('should expose navigation tools', async () => {
      const response = await client.listTools();
      const toolNames = response.tools.map(t => t.name);

      expect(toolNames).toContain('browser_navigate');
      expect(toolNames).toContain('browser_back');
      expect(toolNames).toContain('browser_forward');
      expect(toolNames).toContain('browser_reload');
    });

    it('should expose information tools', async () => {
      const response = await client.listTools();
      const toolNames = response.tools.map(t => t.name);

      expect(toolNames).toContain('browser_get_url');
      expect(toolNames).toContain('browser_get_html');
      expect(toolNames).toContain('browser_get_page_summary');
      expect(toolNames).toContain('browser_get_interactive_elements');
      expect(toolNames).toContain('browser_get_accessibility_tree');
    });

    it('should expose interaction tools', async () => {
      const response = await client.listTools();
      const toolNames = response.tools.map(t => t.name);

      expect(toolNames).toContain('browser_execute_js');
      expect(toolNames).toContain('browser_screenshot');
      expect(toolNames).toContain('browser_get_annotated_screenshot');
    });

    it('should expose tab management tools', async () => {
      const response = await client.listTools();
      const toolNames = response.tools.map(t => t.name);

      expect(toolNames).toContain('window_create_tab');
      expect(toolNames).toContain('window_close_tab');
      expect(toolNames).toContain('window_switch_tab');
    });

    it('should expose POC tool', async () => {
      const response = await client.listTools();
      const toolNames = response.tools.map(t => t.name);

      expect(toolNames).toContain('open_url');
    });

    it('should include tool descriptions', async () => {
      const response = await client.listTools();

      const navigateTool = response.tools.find(t => t.name === 'browser_navigate');
      expect(navigateTool).toBeDefined();
      expect(navigateTool?.description).toBeDefined();
      expect(navigateTool?.description.length).toBeGreaterThan(0);
    });

    it('should include input schemas', async () => {
      const response = await client.listTools();

      const navigateTool = response.tools.find(t => t.name === 'browser_navigate');
      expect(navigateTool).toBeDefined();
      expect(navigateTool?.inputSchema).toBeDefined();
      expect(navigateTool?.inputSchema.type).toBe('object');
      expect(navigateTool?.inputSchema.properties).toBeDefined();
    });
  });

  describe('Tool Execution - Navigation', () => {
    it('should call browser_get_url successfully', async () => {
      const result = await client.callTool({
        name: 'browser_get_url',
        arguments: {}
      });

      expect(result.content).toBeDefined();
      expect(Array.isArray(result.content)).toBe(true);
      expect(result.content.length).toBeGreaterThan(0);
      expect(result.content[0].type).toBe('text');
    });

    it('should call browser_navigate with valid URL', async () => {
      const result = await client.callTool({
        name: 'browser_navigate',
        arguments: {
          url: 'https://example.com'
        }
      });

      expect(result.content).toBeDefined();
      expect(result.isError).toBeUndefined();
      const text = result.content[0].type === 'text' ? result.content[0].text : '';
      expect(text).toContain('Navigated to');
    });

    it('should call browser_reload', async () => {
      const result = await client.callTool({
        name: 'browser_reload',
        arguments: {
          ignoreCache: false
        }
      });

      expect(result.content).toBeDefined();
      expect(result.isError).toBeUndefined();
    });

    it('should call browser_back', async () => {
      const result = await client.callTool({
        name: 'browser_back',
        arguments: {}
      });

      expect(result.content).toBeDefined();
      expect(result.isError).toBeUndefined();
    });

    it('should call browser_forward', async () => {
      const result = await client.callTool({
        name: 'browser_forward',
        arguments: {}
      });

      expect(result.content).toBeDefined();
      expect(result.isError).toBeUndefined();
    });
  });

  describe('Tool Execution - Information', () => {
    it('should get page HTML', async () => {
      const result = await client.callTool({
        name: 'browser_get_html',
        arguments: {}
      });

      expect(result.content).toBeDefined();
      expect(result.content[0].type).toBe('text');
      const text = result.content[0].type === 'text' ? result.content[0].text : '';
      expect(text.length).toBeGreaterThan(0);
    });

    it('should get page summary', async () => {
      const result = await client.callTool({
        name: 'browser_get_page_summary',
        arguments: {}
      });

      expect(result.content).toBeDefined();
      expect(result.isError).toBeUndefined();
      const text = result.content[0].type === 'text' ? result.content[0].text : '';
      expect(text).toContain('Page Summary');
    });

    it('should get interactive elements', async () => {
      const result = await client.callTool({
        name: 'browser_get_interactive_elements',
        arguments: {}
      });

      expect(result.content).toBeDefined();
      expect(result.isError).toBeUndefined();
    });

    it('should get accessibility tree', async () => {
      const result = await client.callTool({
        name: 'browser_get_accessibility_tree',
        arguments: {}
      });

      expect(result.content).toBeDefined();
      expect(result.isError).toBeUndefined();
    });

    it('should query content - forms', async () => {
      const result = await client.callTool({
        name: 'browser_query_content',
        arguments: {
          queryType: 'forms'
        }
      });

      expect(result.content).toBeDefined();
      expect(result.isError).toBeUndefined();
    });

    it('should query content - navigation', async () => {
      const result = await client.callTool({
        name: 'browser_query_content',
        arguments: {
          queryType: 'navigation'
        }
      });

      expect(result.content).toBeDefined();
      expect(result.isError).toBeUndefined();
    });
  });

  describe('Tool Execution - Interaction', () => {
    it('should execute JavaScript', async () => {
      const result = await client.callTool({
        name: 'browser_execute_js',
        arguments: {
          code: 'document.title'
        }
      });

      expect(result.content).toBeDefined();
      expect(result.isError).toBeUndefined();
      const text = result.content[0].type === 'text' ? result.content[0].text : '';
      expect(text).toContain('Result:');
    });

    it('should capture screenshot', async () => {
      const result = await client.callTool({
        name: 'browser_screenshot',
        arguments: {
          fullPage: false
        }
      });

      expect(result.content).toBeDefined();
      expect(result.isError).toBeUndefined();
      // Screenshot should return image content
      const hasImage = result.content.some(c => c.type === 'image');
      expect(hasImage).toBe(true);
    });

    it('should get annotated screenshot', async () => {
      const result = await client.callTool({
        name: 'browser_get_annotated_screenshot',
        arguments: {}
      });

      expect(result.content).toBeDefined();
      expect(result.isError).toBeUndefined();
      // Should have both image and text
      expect(result.content.length).toBeGreaterThan(0);
    });
  });

  describe('Tool Execution - Tab Management', () => {
    it('should create new tab', async () => {
      const result = await client.callTool({
        name: 'window_create_tab',
        arguments: {
          url: 'https://github.com'
        }
      });

      expect(result.content).toBeDefined();
      expect(result.isError).toBeUndefined();
      const text = result.content[0].type === 'text' ? result.content[0].text : '';
      expect(text).toContain('Created new tab');
    });

    it('should switch tabs', async () => {
      const result = await client.callTool({
        name: 'window_switch_tab',
        arguments: {
          tabIndex: 0
        }
      });

      expect(result.content).toBeDefined();
      expect(result.isError).toBeUndefined();
      const text = result.content[0].type === 'text' ? result.content[0].text : '';
      expect(text).toContain('Switched to tab');
    });

    it('should close tab', async () => {
      const result = await client.callTool({
        name: 'window_close_tab',
        arguments: {
          tabIndex: 1
        }
      });

      expect(result.content).toBeDefined();
      expect(result.isError).toBeUndefined();
      const text = result.content[0].type === 'text' ? result.content[0].text : '';
      expect(text).toContain('Closed tab');
    });
  });

  describe('Error Handling', () => {
    it('should handle unknown tool gracefully', async () => {
      await expect(async () => {
        await client.callTool({
          name: 'nonexistent_tool',
          arguments: {}
        });
      }).rejects.toThrow();
    });

    it('should handle invalid URL in navigate', async () => {
      const result = await client.callTool({
        name: 'browser_navigate',
        arguments: {
          url: 'not-a-valid-url'
        }
      });

      // Should return error in content
      expect(result.isError).toBe(true);
    });

    it('should handle missing required arguments', async () => {
      await expect(async () => {
        await client.callTool({
          name: 'browser_navigate',
          arguments: {}
        });
      }).rejects.toThrow();
    });

    it('should handle invalid argument types', async () => {
      await expect(async () => {
        await client.callTool({
          name: 'window_switch_tab',
          arguments: {
            tabIndex: 'invalid' // Should be number
          }
        });
      }).rejects.toThrow();
    });
  });

  describe('POC Tool', () => {
    it('should execute open_url POC tool', async () => {
      const result = await client.callTool({
        name: 'open_url',
        arguments: {
          url: 'https://example.com'
        }
      });

      expect(result.content).toBeDefined();
      const text = result.content[0].type === 'text' ? result.content[0].text : '';
      // POC tool should indicate success or failure
      expect(text.length).toBeGreaterThan(0);
    });
  });

  describe('Response Format', () => {
    it('should return proper content structure', async () => {
      const result = await client.callTool({
        name: 'browser_get_url',
        arguments: {}
      });

      expect(result.content).toBeDefined();
      expect(Array.isArray(result.content)).toBe(true);
      result.content.forEach(item => {
        expect(item).toHaveProperty('type');
        expect(['text', 'image', 'resource']).toContain(item.type);
      });
    });

    it('should include text in text content items', async () => {
      const result = await client.callTool({
        name: 'browser_get_url',
        arguments: {}
      });

      const textContent = result.content.find(c => c.type === 'text');
      expect(textContent).toBeDefined();
      if (textContent && textContent.type === 'text') {
        expect(textContent.text).toBeDefined();
        expect(typeof textContent.text).toBe('string');
      }
    });

    it('should include proper image structure for screenshots', async () => {
      const result = await client.callTool({
        name: 'browser_screenshot',
        arguments: {}
      });

      const imageContent = result.content.find(c => c.type === 'image');
      expect(imageContent).toBeDefined();
      if (imageContent && imageContent.type === 'image') {
        expect(imageContent.data).toBeDefined();
        expect(imageContent.mimeType).toBe('image/png');
      }
    });
  });

  describe('Tool Schema Validation', () => {
    it('should have valid schema for browser_navigate', async () => {
      const response = await client.listTools();
      const tool = response.tools.find(t => t.name === 'browser_navigate');

      expect(tool).toBeDefined();
      expect(tool?.inputSchema).toBeDefined();
      expect(tool?.inputSchema.properties).toHaveProperty('url');
      expect(tool?.inputSchema.properties).toHaveProperty('tabIndex');
    });

    it('should have valid schema for browser_execute_js', async () => {
      const response = await client.listTools();
      const tool = response.tools.find(t => t.name === 'browser_execute_js');

      expect(tool).toBeDefined();
      expect(tool?.inputSchema).toBeDefined();
      expect(tool?.inputSchema.properties).toHaveProperty('code');
      expect(tool?.inputSchema.properties).toHaveProperty('tabIndex');
    });

    it('should have valid schema for browser_query_content', async () => {
      const response = await client.listTools();
      const tool = response.tools.find(t => t.name === 'browser_query_content');

      expect(tool).toBeDefined();
      expect(tool?.inputSchema).toBeDefined();
      expect(tool?.inputSchema.properties).toHaveProperty('queryType');
    });
  });
});
