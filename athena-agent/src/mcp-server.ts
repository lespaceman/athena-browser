/**
 * Athena Browser MCP Server
 *
 * Provides MCP tools for controlling the Athena Browser from Claude.
 * Tools communicate with the C++ GTK application via its existing API.
 */

import { tool, createSdkMcpServer } from '@anthropic-ai/claude-agent-sdk';
import { z } from 'zod';
import http from 'http';
import { URLSearchParams } from 'url';
import { Logger } from './logger.js';

const logger = new Logger('MCPServer');

function toolError(prefix: string, error: unknown): string {
  const message = error instanceof Error ? error.message : String(error);
  return `${prefix}: ${message}`;
}

/**
 * Browser API base configuration
 */
let browserApiSocketPath: string | null = null;

/**
 * Set the Express API socket path (Unix domain socket exposed by the agent).
 */
export function setBrowserApiBase(socketPath: string) {
  browserApiSocketPath = socketPath;
  logger.info('Browser API socket configured for MCP tools', {
    socketPath,
    note: 'Requests hit the agent Express server, which proxies to the native controller'
  });
}

/**
 * Make an HTTP call to the browser backend via Unix socket
 */
async function callBrowserApi(
  endpoint: string,
  method: string = 'GET',
  body?: any,
  queryParams?: Record<string, string | number | boolean | undefined>
): Promise<any> {
  if (!browserApiSocketPath) {
    throw new Error('Browser API socket path not configured. Call setBrowserApiBase() first.');
  }

  return new Promise((resolve, reject) => {
    const sanitizedEndpoint = endpoint.startsWith('/v1')
      ? endpoint
      : `/v1${endpoint.startsWith('/') ? endpoint : `/${endpoint}`}`;

    const filteredBody =
      body && typeof body === 'object'
        ? Object.fromEntries(
            Object.entries(body).filter(([, value]) => value !== undefined)
          )
        : undefined;
    const bodyStr = filteredBody ? JSON.stringify(filteredBody) : '';

    let queryString = '';
    if (queryParams) {
      const params = new URLSearchParams();
      for (const [key, value] of Object.entries(queryParams)) {
        if (value !== undefined) {
          params.append(key, String(value));
        }
      }
      const serialized = params.toString();
      if (serialized.length > 0) {
        queryString = `?${serialized}`;
      }
    }

    const options: http.RequestOptions = {
      socketPath: browserApiSocketPath!, // Already checked for null above
      path: sanitizedEndpoint + queryString,
      method: method,
      headers: {
        'Accept': 'application/json',
        'User-Agent': 'Athena-MCP/1.0',
        ...(bodyStr
          ? {
              'Content-Type': 'application/json',
              'Content-Length': Buffer.byteLength(bodyStr)
            }
          : {})
      }
    };

    logger.debug('Browser API call', {
      endpoint: options.path,
      method,
      hasBody: Boolean(bodyStr)
    });

    const req = http.request(options, (res) => {
      let data = '';

      res.on('data', (chunk) => {
        data += chunk;
      });

      res.on('end', () => {
        try {
          const result = JSON.parse(data);

          if (res.statusCode && res.statusCode >= 200 && res.statusCode < 300) {
            if (result && typeof result === 'object' && result.success === false) {
              const message = result.error || result.message || 'Unknown error';
              logger.error('Browser API logical error', {
                endpoint: options.path,
                statusCode: res.statusCode,
                error: message
              });
              reject(new Error(message));
              return;
            }

            logger.debug('Browser API success', {
              endpoint: options.path,
              statusCode: res.statusCode
            });
            resolve(result);
          } else {
            logger.error('Browser API error', {
              endpoint: options.path,
              statusCode: res.statusCode,
              error: result.error || result.message
            });
            reject(new Error(result.error || result.message || 'Unknown error'));
          }
        } catch (error) {
          logger.error('Browser API parse error', {
            endpoint: options.path,
            error: String(error)
          });
          reject(new Error('Failed to parse response: ' + data));
        }
      });
    });

    req.on('error', (error) => {
      logger.error('Browser API request error', {
        endpoint: options.path,
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
            const result = await callBrowserApi('/poc/open_url', 'POST', { url: args.url });

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
          const result = await callBrowserApi('/browser/navigate', 'POST', {
            url: args.url,
            tabIndex: args.tabIndex
          });

          return {
            content: [{
              type: 'text',
              text: `Navigated to ${result.finalUrl ?? args.url} (tab ${result.tabIndex}, ${result.loadTimeMs ?? '—'} ms)`
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
          const result = await callBrowserApi('/browser/back', 'POST', {
            tabIndex: args.tabIndex
          });

          return {
            content: [{
              type: 'text',
              text: `Went back to ${result.finalUrl ?? 'previous page'} (tab ${result.tabIndex}, ${result.loadTimeMs ?? '—'} ms)`
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
          const result = await callBrowserApi('/browser/forward', 'POST', {
            tabIndex: args.tabIndex
          });

          return {
            content: [{
              type: 'text',
              text: `Moved forward to ${result.finalUrl ?? 'next page'} (tab ${result.tabIndex}, ${result.loadTimeMs ?? '—'} ms)`
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
          const result = await callBrowserApi('/browser/reload', 'POST', {
            tabIndex: args.tabIndex,
            ignoreCache: args.ignoreCache
          });

          return {
            content: [{
              type: 'text',
              text: `${args.ignoreCache ? 'Hard reload' : 'Reload'} completed on tab ${result.tabIndex} (${result.loadTimeMs ?? '—'} ms, ${result.finalUrl ?? 'current URL'})`
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
          const result = await callBrowserApi('/browser/url', 'GET', undefined, {
            tabIndex: args.tabIndex
          });

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
          try {
            const result = await callBrowserApi('/browser/html', 'GET', undefined, {
              tabIndex: args.tabIndex
            });

            return {
              content: [{
                type: 'text',
                text: result.html ?? ''
              }]
            };
          } catch (error) {
            logger.error('browser_get_html failed', {
              error: error instanceof Error ? error.message : String(error)
            });
            return {
              content: [{
                type: 'text',
                text: toolError('✗ Failed to fetch HTML', error)
              }],
              isError: true
            };
          }
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
          try {
            const result = await callBrowserApi('/browser/execute-js', 'POST', {
              code: args.code,
              tabIndex: args.tabIndex
            });

            return {
              content: [{
                type: 'text',
                text: `JavaScript executed. Result: ${result.result ?? 'undefined'}`
              }]
            };
          } catch (error) {
            logger.error('browser_execute_js failed', {
              error: error instanceof Error ? error.message : String(error)
            });
            return {
              content: [{
                type: 'text',
                text: toolError('✗ Failed to execute JavaScript', error)
              }],
              isError: true
            };
          }
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
          try {
            const result = await callBrowserApi('/browser/screenshot', 'POST', {
              tabIndex: args.tabIndex,
              fullPage: args.fullPage
            });

            return {
              content: [{
                type: 'image',
                data: result.screenshot,
                mimeType: 'image/png'
              }]
            };
          } catch (error) {
            logger.error('browser_screenshot failed', {
              error: error instanceof Error ? error.message : String(error)
            });
            return {
              content: [{
                type: 'text',
                text: toolError('✗ Failed to capture screenshot', error)
              }],
              isError: true
            };
          }
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
          const result = await callBrowserApi('/window/create', 'POST', { url: args.url });

          return {
            content: [{
              type: 'text',
              text: `Created new tab at index ${result.tabIndex} with URL: ${args.url}`
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
          await callBrowserApi('/window/close', 'POST', { tabIndex: args.tabIndex });

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
          await callBrowserApi('/window/switch', 'POST', { tabIndex: args.tabIndex });

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
