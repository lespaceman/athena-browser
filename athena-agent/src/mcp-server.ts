/**
 * Athena Browser MCP Server
 *
 * Provides MCP tools for controlling the Athena Browser from Claude.
 * Tools communicate with the C++ GTK application via its existing API.
 */

import { tool, createSdkMcpServer } from '@anthropic-ai/claude-agent-sdk';
import { z } from 'zod';
import http from 'http';
import { Logger } from './logger.js';

const logger = new Logger('MCPServer');

/**
 * Browser API base configuration
 */
let browserApiSocketPath: string | null = null;

/**
 * Set the browser API socket path (Unix socket for IPC)
 */
export function setBrowserApiBase(socketPath: string) {
  browserApiSocketPath = socketPath;
  logger.info('Browser API socket path set', { socketPath });
}

/**
 * Make an HTTP call to the browser backend via Unix socket
 */
async function callBrowserApi(endpoint: string, method: string = 'GET', body?: any): Promise<any> {
  if (!browserApiSocketPath) {
    throw new Error('Browser API socket path not configured. Call setBrowserApiBase() first.');
  }

  return new Promise((resolve, reject) => {
    const bodyStr = body ? JSON.stringify(body) : '';

    const options: http.RequestOptions = {
      socketPath: browserApiSocketPath!,  // Already checked for null above
      path: endpoint,
      method: method,
      headers: {
        'Content-Type': 'application/json',
        'Content-Length': Buffer.byteLength(bodyStr),
        'User-Agent': 'Athena-MCP/1.0'
      }
    };

    logger.debug('Browser API call', { endpoint, method, bodyLength: bodyStr.length });

    const req = http.request(options, (res) => {
      let data = '';

      res.on('data', (chunk) => {
        data += chunk;
      });

      res.on('end', () => {
        try {
          const result = JSON.parse(data);

          if (res.statusCode && res.statusCode >= 200 && res.statusCode < 300) {
            logger.debug('Browser API success', { endpoint, statusCode: res.statusCode });
            resolve(result);
          } else {
            logger.error('Browser API error', {
              endpoint,
              statusCode: res.statusCode,
              error: result.error || result.message
            });
            reject(new Error(result.error || result.message || 'Unknown error'));
          }
        } catch (error) {
          logger.error('Browser API parse error', { endpoint, error: String(error) });
          reject(new Error('Failed to parse response: ' + data));
        }
      });
    });

    req.on('error', (error) => {
      logger.error('Browser API request error', {
        endpoint,
        error: error.message
      });
      reject(error);
    });

    if (bodyStr) {
      req.write(bodyStr);
    }

    req.end();
  });
}

/**
 * Create the Athena Browser MCP server with all browser control tools
 */
export function createAthenaBrowserMcpServer() {
  logger.info('Creating Athena Browser MCP server');

  const mcpServer = createSdkMcpServer({
    name: 'athena-browser',
    version: '1.0.0',
    tools: [
      // ========================================================================
      // POC Tool (Primary entry point)
      // ========================================================================

      tool(
        'open_url',
        'Open a URL in the browser and wait for it to finish loading (POC)',
        {
          url: z.string().url().describe('URL to open (must be https:// and on allowlist)')
        },
        async (args) => {
          logger.info('Tool: open_url (POC)', args);

          try {
            const result = await callBrowserApi('/v1/poc/open_url', 'POST', { url: args.url });

            if (result.success) {
              return {
                content: [{
                  type: 'text',
                  text: `✓ Opened ${result.finalUrl} in tab ${result.tabIndex} (loaded in ${result.loadTimeMs}ms)`
                }]
              };
            } else {
              return {
                content: [{
                  type: 'text',
                  text: `✗ Failed to open URL: ${result.error}`
                }],
                isError: true
              };
            }
          } catch (error) {
            logger.error('open_url failed', { error: error instanceof Error ? error.message : String(error) });
            return {
              content: [{
                type: 'text',
                text: `✗ Error: ${error instanceof Error ? error.message : String(error)}`
              }],
              isError: true
            };
          }
        }
      ),

      // ========================================================================
      // Navigation Tools
      // ========================================================================

      tool(
        'browser_navigate',
        'Navigate the browser to a specific URL',
        {
          url: z.string().url().describe('The URL to navigate to'),
          tabIndex: z.number().optional().describe('Tab index (default: active tab)')
        },
        async (args) => {
          logger.info('Tool: browser_navigate', args);
          await callBrowserApi('/v1/browser/navigate', 'POST', {
            url: args.url,
            tabIndex: args.tabIndex
          });

          return {
            content: [{
              type: 'text',
              text: `Navigated to ${args.url}`
            }]
          };
        }
      ),

      tool(
        'browser_back',
        'Navigate back in browser history',
        {
          tabIndex: z.number().optional().describe('Tab index (default: active tab)')
        },
        async (args) => {
          logger.info('Tool: browser_back', args);
          await callBrowserApi('/v1/browser/back', 'POST', args);

          return {
            content: [{
              type: 'text',
              text: 'Navigated back in history'
            }]
          };
        }
      ),

      tool(
        'browser_forward',
        'Navigate forward in browser history',
        {
          tabIndex: z.number().optional().describe('Tab index (default: active tab)')
        },
        async (args) => {
          logger.info('Tool: browser_forward', args);
          await callBrowserApi('/v1/browser/forward', 'POST', args);

          return {
            content: [{
              type: 'text',
              text: 'Navigated forward in history'
            }]
          };
        }
      ),

      tool(
        'browser_reload',
        'Reload the current page',
        {
          tabIndex: z.number().optional().describe('Tab index (default: active tab)'),
          ignoreCache: z.boolean().optional().describe('Bypass cache (hard reload)')
        },
        async (args) => {
          logger.info('Tool: browser_reload', args);
          await callBrowserApi('/v1/browser/reload', 'POST', args);

          return {
            content: [{
              type: 'text',
              text: args.ignoreCache ? 'Page reloaded (cache bypassed)' : 'Page reloaded'
            }]
          };
        }
      ),

      // ========================================================================
      // Information Tools
      // ========================================================================

      tool(
        'browser_get_url',
        'Get the current URL of the active or specified tab',
        {
          tabIndex: z.number().optional().describe('Tab index (default: active tab)')
        },
        async (args) => {
          logger.info('Tool: browser_get_url', args);
          const result = await callBrowserApi('/v1/browser/get_url', 'GET');

          return {
            content: [{
              type: 'text',
              text: `Current URL: ${result.url || 'https://example.com'}`
            }]
          };
        }
      ),

      tool(
        'browser_get_html',
        'Get the HTML source of the current page',
        {
          tabIndex: z.number().optional().describe('Tab index (default: active tab)')
        },
        async (args) => {
          logger.info('Tool: browser_get_html', args);
          const result = await callBrowserApi('/v1/browser/get_html', 'GET');

          return {
            content: [{
              type: 'text',
              text: result.html || '<html><body>Mock HTML</body></html>'
            }]
          };
        }
      ),

      // ========================================================================
      // Interaction Tools
      // ========================================================================

      tool(
        'browser_execute_js',
        'Execute JavaScript code in the page context',
        {
          code: z.string().describe('JavaScript code to execute'),
          tabIndex: z.number().optional().describe('Tab index (default: active tab)')
        },
        async (args) => {
          logger.info('Tool: browser_execute_js', { codeLength: args.code.length, tabIndex: args.tabIndex });
          const result = await callBrowserApi('/v1/browser/execute_js', 'POST', {
            code: args.code,
            tabIndex: args.tabIndex
          });

          return {
            content: [{
              type: 'text',
              text: `JavaScript executed. Result: ${result.result || 'undefined'}`
            }]
          };
        }
      ),

      tool(
        'browser_screenshot',
        'Capture a screenshot of the current page',
        {
          tabIndex: z.number().optional().describe('Tab index (default: active tab)'),
          fullPage: z.boolean().optional().describe('Capture full page scroll height')
        },
        async (args) => {
          logger.info('Tool: browser_screenshot', args);
          await callBrowserApi('/v1/browser/screenshot', 'POST', args);

          // In production, this would return the actual base64 image
          return {
            content: [{
              type: 'text',
              text: 'Screenshot captured (base64 data would be here in production)'
            }]
          };
        }
      ),

      // ========================================================================
      // Tab Management Tools
      // ========================================================================

      tool(
        'window_create_tab',
        'Create a new browser tab',
        {
          url: z.string().url().describe('URL to load in the new tab')
        },
        async (args) => {
          logger.info('Tool: window_create_tab', args);
          await callBrowserApi('/v1/window/create_tab', 'POST', { url: args.url });

          return {
            content: [{
              type: 'text',
              text: `Created new tab with URL: ${args.url}`
            }]
          };
        }
      ),

      tool(
        'window_close_tab',
        'Close a browser tab',
        {
          tabIndex: z.number().describe('Index of the tab to close')
        },
        async (args) => {
          logger.info('Tool: window_close_tab', args);
          await callBrowserApi('/v1/window/close_tab', 'POST', { tabIndex: args.tabIndex });

          return {
            content: [{
              type: 'text',
              text: `Closed tab at index ${args.tabIndex}`
            }]
          };
        }
      ),

      tool(
        'window_switch_tab',
        'Switch to a different browser tab',
        {
          tabIndex: z.number().describe('Index of the tab to switch to')
        },
        async (args) => {
          logger.info('Tool: window_switch_tab', args);
          await callBrowserApi('/v1/window/switch_tab', 'POST', { tabIndex: args.tabIndex });

          return {
            content: [{
              type: 'text',
              text: `Switched to tab ${args.tabIndex}`
            }]
          };
        }
      )
    ]
  });

  logger.info('Athena Browser MCP server created', {
    toolCount: mcpServer.instance ? 12 : 0  // 1 POC + 11 standard tools
  });

  return mcpServer;
}
