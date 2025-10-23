/**
 * Athena Browser MCP Server
 *
 * Official MCP SDK implementation following best practices:
 * - Uses McpServer from @modelcontextprotocol/sdk
 * - Proper tool registration with Zod schemas
 * - Structured content for machine-readable responses
 * - Clean separation of concerns
 * - Supports both stdio and HTTP transports
 */

import { McpServer } from '@modelcontextprotocol/sdk/server/mcp.js';
import { z } from 'zod';
import http from 'http';
import { URLSearchParams } from 'url';
import { Logger } from './logger';
import { config } from './config';

const logger = new Logger('MCPServer');

/**
 * Browser API configuration
 */
let browserApiSocketPath: string | null = null;

/**
 * Set the browser API socket path for making HTTP calls
 */
export function setBrowserApiBase(socketPath: string) {
  browserApiSocketPath = socketPath;
  logger.info('Browser API socket configured', { socketPath });
}

/**
 * Make HTTP call to the browser backend via Unix socket
 */
interface BrowserApiCallResponse {
  [key: string]: unknown;
}

async function callBrowserApi(
  endpoint: string,
  method: string = 'GET',
  body?: Record<string, unknown>,
  queryParams?: Record<string, string | number | boolean | undefined>,
  timeoutMs: number = 30000 // Default 30s timeout for screenshot operations
): Promise<BrowserApiCallResponse> {
  if (!browserApiSocketPath) {
    throw new Error('Browser API socket not configured. Call setBrowserApiBase() first.');
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
      socketPath: browserApiSocketPath!,
      path: sanitizedEndpoint + queryString,
      method: method,
      headers: {
        'Accept': 'application/json',
        'User-Agent': 'Athena-MCP/2.0',
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

    req.on('timeout', () => {
      req.destroy();
      const error = new Error(`Request timeout after ${timeoutMs}ms for ${endpoint}`);
      logger.error('Browser API request timeout', {
        endpoint: options.path,
        timeoutMs
      });
      reject(error);
    });

    // Set timeout on the request
    req.setTimeout(timeoutMs);

    if (bodyStr) {
      req.write(bodyStr);
    }

    req.end();
  });
}

/**
 * Create and configure the Athena Browser MCP server
 *
 * Following official MCP SDK patterns:
 * - Uses McpServer instead of custom wrapper
 * - Registers tools with registerTool() API
 * - Provides structured content for machine-readable responses
 * - Consistent error handling with isError flag
 */
export function createAthenaBrowserMcpServer(): McpServer {
  logger.info('Creating Athena Browser MCP server');

  const server = new McpServer({
    name: 'athena-browser',
    version: '2.0.0'
  });

  // ============================================================================
  // Navigation Tools
  // ============================================================================

  server.registerTool(
    'browser_navigate',
    {
      title: 'Navigate Browser',
      description: 'Navigate the browser to a specific URL',
      inputSchema: {
        url: z.string().url().describe('The URL to navigate to'),
        tabIndex: z.number().optional().describe('Tab index (default: active tab)')
      },
      outputSchema: {
        finalUrl: z.string(),
        tabIndex: z.number(),
        loadTimeMs: z.number()
      }
    },
    async ({ url, tabIndex }) => {
      logger.info('Tool: browser_navigate', { url, tabIndex });
      try {
        const result = await callBrowserApi('/browser/navigate', 'POST', {
          url,
          tabIndex
        });

        const output = {
          finalUrl: result.finalUrl ?? url,
          tabIndex: result.tabIndex ?? 0,
          loadTimeMs: result.loadTimeMs ?? 0
        };

        return {
          content: [{
            type: 'text',
            text: `Navigated to ${output.finalUrl} (tab ${output.tabIndex}, ${output.loadTimeMs}ms)`
          }],
          structuredContent: output
        };
      } catch (error) {
        const message = error instanceof Error ? error.message : String(error);
        logger.error('Tool error: browser_navigate', { error: message });
        return {
          content: [{
            type: 'text',
            text: `Navigation failed: ${message}`
          }],
          isError: true
        };
      }
    }
  );

  server.registerTool(
    'browser_back',
    {
      title: 'Navigate Back',
      description: 'Navigate back in browser history',
      inputSchema: {
        tabIndex: z.number().optional().describe('Tab index (default: active tab)')
      },
      outputSchema: {
        finalUrl: z.string(),
        tabIndex: z.number(),
        loadTimeMs: z.number()
      }
    },
    async ({ tabIndex }) => {
      logger.info('Tool: browser_back', { tabIndex });
      try {
        const result = await callBrowserApi('/browser/back', 'POST', { tabIndex });

        const output = {
          finalUrl: result.finalUrl ?? 'previous page',
          tabIndex: result.tabIndex ?? 0,
          loadTimeMs: result.loadTimeMs ?? 0
        };

        return {
          content: [{
            type: 'text',
            text: `Went back to ${output.finalUrl} (tab ${output.tabIndex}, ${output.loadTimeMs}ms)`
          }],
          structuredContent: output
        };
      } catch (error) {
        const message = error instanceof Error ? error.message : String(error);
        logger.error('Tool error: browser_back', { error: message });
        return {
          content: [{
            type: 'text',
            text: `Back navigation failed: ${message}`
          }],
          isError: true
        };
      }
    }
  );

  server.registerTool(
    'browser_forward',
    {
      title: 'Navigate Forward',
      description: 'Navigate forward in browser history',
      inputSchema: {
        tabIndex: z.number().optional().describe('Tab index (default: active tab)')
      },
      outputSchema: {
        finalUrl: z.string(),
        tabIndex: z.number(),
        loadTimeMs: z.number()
      }
    },
    async ({ tabIndex }) => {
      logger.info('Tool: browser_forward', { tabIndex });
      try {
        const result = await callBrowserApi('/browser/forward', 'POST', { tabIndex });

        const output = {
          finalUrl: result.finalUrl ?? 'next page',
          tabIndex: result.tabIndex ?? 0,
          loadTimeMs: result.loadTimeMs ?? 0
        };

        return {
          content: [{
            type: 'text',
            text: `Moved forward to ${output.finalUrl} (tab ${output.tabIndex}, ${output.loadTimeMs}ms)`
          }],
          structuredContent: output
        };
      } catch (error) {
        const message = error instanceof Error ? error.message : String(error);
        logger.error('Tool error: browser_forward', { error: message });
        return {
          content: [{
            type: 'text',
            text: `Forward navigation failed: ${message}`
          }],
          isError: true
        };
      }
    }
  );

  server.registerTool(
    'browser_reload',
    {
      title: 'Reload Page',
      description: 'Reload the current page',
      inputSchema: {
        tabIndex: z.number().optional().describe('Tab index (default: active tab)'),
        ignoreCache: z.boolean().optional().describe('Bypass cache (hard reload)')
      },
      outputSchema: {
        tabIndex: z.number(),
        finalUrl: z.string(),
        loadTimeMs: z.number()
      }
    },
    async ({ tabIndex, ignoreCache }) => {
      logger.info('Tool: browser_reload', { tabIndex, ignoreCache });
      try {
        const result = await callBrowserApi('/browser/reload', 'POST', {
          tabIndex,
          ignoreCache
        });

        const output = {
          tabIndex: result.tabIndex ?? 0,
          finalUrl: result.finalUrl ?? 'current URL',
          loadTimeMs: result.loadTimeMs ?? 0
        };

        return {
          content: [{
            type: 'text',
            text: `${ignoreCache ? 'Hard reload' : 'Reload'} completed on tab ${output.tabIndex} (${output.loadTimeMs}ms, ${output.finalUrl})`
          }],
          structuredContent: output
        };
      } catch (error) {
        const message = error instanceof Error ? error.message : String(error);
        logger.error('Tool error: browser_reload', { error: message });
        return {
          content: [{
            type: 'text',
            text: `Reload failed: ${message}`
          }],
          isError: true
        };
      }
    }
  );

  // ============================================================================
  // Information Tools
  // ============================================================================

  server.registerTool(
    'browser_get_url',
    {
      title: 'Get Current URL',
      description: 'Get the current URL of the active or specified tab',
      inputSchema: {
        tabIndex: z.number().optional().describe('Tab index (default: active tab)')
      },
      outputSchema: {
        url: z.string()
      }
    },
    async ({ tabIndex }) => {
      logger.info('Tool: browser_get_url', { tabIndex });
      try {
        const result = await callBrowserApi('/browser/url', 'GET', undefined, { tabIndex });

        const output = {
          url: result.url || 'https://example.com'
        };

        return {
          content: [{
            type: 'text',
            text: `Current URL: ${output.url}`
          }],
          structuredContent: output
        };
      } catch (error) {
        const message = error instanceof Error ? error.message : String(error);
        logger.error('Tool error: browser_get_url', { error: message });
        return {
          content: [{
            type: 'text',
            text: `Failed to fetch URL: ${message}`
          }],
          isError: true
        };
      }
    }
  );

  server.registerTool(
    'browser_get_html',
    {
      title: 'Get Page HTML',
      description: 'Get the HTML source of the current page',
      inputSchema: {
        tabIndex: z.number().optional().describe('Tab index (default: active tab)')
      },
      outputSchema: {
        html: z.string()
      }
    },
    async ({ tabIndex }) => {
      logger.info('Tool: browser_get_html', { tabIndex });
      try {
        const result = await callBrowserApi('/browser/html', 'GET', undefined, { tabIndex });

        const html = typeof result.html === 'string' ? result.html : '';
        const output = {
          html
        };

        return {
          content: [{
            type: 'text',
            text: html
          }],
          structuredContent: output
        };
      } catch (error) {
        const message = error instanceof Error ? error.message : String(error);
        logger.error('Tool error: browser_get_html', { error: message });
        return {
          content: [{
            type: 'text',
            text: `Failed to fetch HTML: ${message}`
          }],
          isError: true
        };
      }
    }
  );

  server.registerTool(
    'browser_get_page_summary',
    {
      title: 'Get Page Summary',
      description: 'Get a compact summary of the current page (title, headings, element counts). Much smaller than full HTML (~1-2KB vs 100KB+).',
      inputSchema: {
        tabIndex: z.number().optional().describe('Tab index (default: active tab)')
      },
      outputSchema: {
        title: z.string(),
        url: z.string(),
        headings: z.array(z.string()),
        forms: z.number(),
        links: z.number(),
        buttons: z.number(),
        inputs: z.number(),
        images: z.number(),
        mainText: z.string()
      }
    },
    async ({ tabIndex }) => {
      logger.info('Tool: browser_get_page_summary', { tabIndex });
      try {
        const result = await callBrowserApi('/browser/page-summary', 'GET', undefined, { tabIndex });

        const summary = (result.summary && typeof result.summary === 'object' ? result.summary : {}) as Record<string, unknown>;
        const output = {
          title: typeof summary.title === 'string' ? summary.title : 'N/A',
          url: typeof summary.url === 'string' ? summary.url : 'N/A',
          headings: Array.isArray(summary.headings) ? summary.headings as string[] : [],
          forms: typeof summary.forms === 'number' ? summary.forms : 0,
          links: typeof summary.links === 'number' ? summary.links : 0,
          buttons: typeof summary.buttons === 'number' ? summary.buttons : 0,
          inputs: typeof summary.inputs === 'number' ? summary.inputs : 0,
          images: typeof summary.images === 'number' ? summary.images : 0,
          mainText: typeof summary.mainText === 'string' ? summary.mainText : 'N/A'
        };

        const text = `Page Summary:
Title: ${output.title}
URL: ${output.url}
Headings: ${output.headings.length} (${output.headings.slice(0, 3).join(', ')}${output.headings.length > 3 ? '...' : ''})
Forms: ${output.forms}
Links: ${output.links}
Buttons: ${output.buttons}
Inputs: ${output.inputs}
Images: ${output.images}

Main Content Preview:
${output.mainText}`;

        return {
          content: [{
            type: 'text',
            text: text
          }],
          structuredContent: output
        };
      } catch (error) {
        const message = error instanceof Error ? error.message : String(error);
        logger.error('Tool error: browser_get_page_summary', { error: message });
        return {
          content: [{
            type: 'text',
            text: `Failed to fetch page summary: ${message}`
          }],
          isError: true
        };
      }
    }
  );

  server.registerTool(
    'browser_get_interactive_elements',
    {
      title: 'Get Interactive Elements',
      description: 'Get all clickable elements on the page with their positions and attributes. Returns only visible, actionable elements (links, buttons, inputs, etc.). Typical size: 5-20KB for complex pages.',
      inputSchema: {
        tabIndex: z.number().optional().describe('Tab index (default: active tab)')
      },
      outputSchema: {
        elements: z.array(z.object({
          index: z.number(),
          tag: z.string(),
          type: z.string(),
          id: z.string(),
          className: z.string(),
          text: z.string(),
          href: z.string(),
          name: z.string(),
          placeholder: z.string(),
          value: z.string(),
          ariaLabel: z.string(),
          role: z.string(),
          disabled: z.boolean(),
          checked: z.boolean(),
          bounds: z.object({
            x: z.number(),
            y: z.number(),
            width: z.number(),
            height: z.number()
          })
        }))
      }
    },
    async ({ tabIndex }) => {
      logger.info('Tool: browser_get_interactive_elements', { tabIndex });
      try {
        const result = await callBrowserApi('/browser/interactive-elements', 'GET', undefined, { tabIndex });

        const elements = Array.isArray(result.elements) ? result.elements : [];
        const output = { elements };

        const text = `Found ${elements.length} interactive elements:
${elements.slice(0, 10).map((el: { tag: string; type?: string; text?: string; ariaLabel?: string; href?: string }, i: number) =>
  `${i + 1}. ${el.tag}${el.type ? `[type="${el.type}"]` : ''}: ${el.text || el.ariaLabel || el.href || 'N/A'}`
).join('\n')}${elements.length > 10 ? `\n... and ${elements.length - 10} more` : ''}`;

        return {
          content: [{
            type: 'text',
            text: text
          }],
          structuredContent: output
        };
      } catch (error) {
        const message = error instanceof Error ? error.message : String(error);
        logger.error('Tool error: browser_get_interactive_elements', { error: message });
        return {
          content: [{
            type: 'text',
            text: `Failed to fetch interactive elements: ${message}`
          }],
          isError: true
        };
      }
    }
  );

  server.registerTool(
    'browser_get_accessibility_tree',
    {
      title: 'Get Accessibility Tree',
      description: 'Get semantic page structure (reduced DOM) using accessibility tree. Provides semantic structure without full HTML. Typical size: 10-30KB.',
      inputSchema: {
        tabIndex: z.number().optional().describe('Tab index (default: active tab)')
      },
      outputSchema: {
        tree: z.any()
      }
    },
    async ({ tabIndex }) => {
      logger.info('Tool: browser_get_accessibility_tree', { tabIndex });
      try {
        const result = await callBrowserApi('/browser/accessibility-tree', 'GET', undefined, { tabIndex });

        const output = {
          tree: result.tree || {}
        };

        return {
          content: [{
            type: 'text',
            text: JSON.stringify(output.tree, null, 2)
          }],
          structuredContent: output
        };
      } catch (error) {
        const message = error instanceof Error ? error.message : String(error);
        logger.error('Tool error: browser_get_accessibility_tree', { error: message });
        return {
          content: [{
            type: 'text',
            text: `Failed to fetch accessibility tree: ${message}`
          }],
          isError: true
        };
      }
    }
  );

  server.registerTool(
    'browser_query_content',
    {
      title: 'Query Content',
      description: 'Query specific content types from the page. Available types: "forms", "navigation", "article", "tables", "media". Returns only the requested content, much smaller than full HTML.',
      inputSchema: {
        queryType: z.enum(['forms', 'navigation', 'article', 'tables', 'media']).describe('Type of content to query'),
        tabIndex: z.number().optional().describe('Tab index (default: active tab)')
      },
      outputSchema: {
        data: z.any()
      }
    },
    async ({ queryType, tabIndex }) => {
      logger.info('Tool: browser_query_content', { queryType, tabIndex });
      try {
        const result = await callBrowserApi('/browser/query-content', 'POST', {
          queryType,
          tabIndex
        });

        const output = {
          data: result.data || {}
        };

        return {
          content: [{
            type: 'text',
            text: JSON.stringify(output.data, null, 2)
          }],
          structuredContent: output
        };
      } catch (error) {
        const message = error instanceof Error ? error.message : String(error);
        logger.error('Tool error: browser_query_content', { error: message });
        return {
          content: [{
            type: 'text',
            text: `Failed to query content: ${message}`
          }],
          isError: true
        };
      }
    }
  );

  server.registerTool(
    'browser_get_annotated_screenshot',
    {
      title: 'Get Annotated Screenshot',
      description: 'Get screenshot with interactive element annotations. Returns base64 screenshot + array of element positions. Useful for vision-based interactions. Note: Large screenshots may take 30-90s to transfer.',
      inputSchema: {
        tabIndex: z.number().optional().describe('Tab index (default: active tab)'),
        quality: z.number().min(1).max(100).optional().describe('Image quality (1-100, default: 85). Lower values (60-80) reduce file size and transfer time.'),
        maxWidth: z.number().optional().describe('Maximum width in pixels. Image will be scaled down proportionally if larger.'),
        maxHeight: z.number().optional().describe('Maximum height in pixels. Image will be scaled down proportionally if larger.')
      },
      outputSchema: {
        screenshot: z.string(),
        elements: z.array(z.object({
          index: z.number(),
          x: z.number(),
          y: z.number(),
          width: z.number(),
          height: z.number(),
          tag: z.string(),
          text: z.string(),
          type: z.string()
        }))
      }
    },
    async ({ tabIndex, quality, maxWidth, maxHeight }) => {
      logger.info('Tool: browser_get_annotated_screenshot', { tabIndex, quality, maxWidth, maxHeight });
      try {
        const screenshotTimeout = config.screenshotTimeoutMs || 90000;
        const result = await callBrowserApi(
          '/browser/annotated-screenshot',
          'GET',
          undefined,
          { tabIndex, quality, maxWidth, maxHeight },
          screenshotTimeout
        );

        const screenshot = typeof result.screenshot === 'string' ? result.screenshot : '';
        const elements = Array.isArray(result.elements) ? result.elements : [];

        const output = {
          screenshot,
          elements
        };

        return {
          content: [
            {
              type: 'image',
              data: screenshot,
              mimeType: 'image/png'
            },
            {
              type: 'text',
              text: `Annotated ${elements.length} interactive elements on screenshot`
            }
          ],
          structuredContent: output
        };
      } catch (error) {
        const message = error instanceof Error ? error.message : String(error);
        logger.error('Tool error: browser_get_annotated_screenshot', { error: message });
        return {
          content: [{
            type: 'text',
            text: `Failed to get annotated screenshot: ${message}`
          }],
          isError: true
        };
      }
    }
  );

  // ============================================================================
  // Interaction Tools
  // ============================================================================

  server.registerTool(
    'browser_execute_js',
    {
      title: 'Execute JavaScript',
      description: 'Execute JavaScript code in the page context',
      inputSchema: {
        code: z.string().describe('JavaScript code to execute'),
        tabIndex: z.number().optional().describe('Tab index (default: active tab)')
      },
      outputSchema: {
        result: z.any()
      }
    },
    async ({ code, tabIndex }) => {
      logger.info('Tool: browser_execute_js', { codeLength: code.length, tabIndex });
      try {
        const result = await callBrowserApi('/browser/execute-js', 'POST', {
          code,
          tabIndex
        });

        const output = {
          result: result.result ?? 'undefined'
        };

        return {
          content: [{
            type: 'text',
            text: `JavaScript executed. Result: ${output.result}`
          }],
          structuredContent: output
        };
      } catch (error) {
        const message = error instanceof Error ? error.message : String(error);
        logger.error('Tool error: browser_execute_js', { error: message });
        return {
          content: [{
            type: 'text',
            text: `Failed to execute JavaScript: ${message}`
          }],
          isError: true
        };
      }
    }
  );

  server.registerTool(
    'browser_screenshot',
    {
      title: 'Capture Screenshot',
      description: 'Capture a screenshot of the current page. Note: Large screenshots may take 30-90s to transfer. Use quality parameter to reduce file size.',
      inputSchema: {
        tabIndex: z.number().optional().describe('Tab index (default: active tab)'),
        fullPage: z.boolean().optional().describe('Capture full page scroll height'),
        quality: z.number().min(1).max(100).optional().describe('Image quality (1-100, default: 85). Lower values (60-80) reduce file size and transfer time.'),
        maxWidth: z.number().optional().describe('Maximum width in pixels. Image will be scaled down proportionally if larger.'),
        maxHeight: z.number().optional().describe('Maximum height in pixels. Image will be scaled down proportionally if larger.')
      },
      outputSchema: {
        screenshot: z.string()
      }
    },
    async ({ tabIndex, fullPage, quality, maxWidth, maxHeight }) => {
      logger.info('Tool: browser_screenshot', { tabIndex, fullPage, quality, maxWidth, maxHeight });
      try {
        const screenshotTimeout = config.screenshotTimeoutMs || 90000;
        const result = await callBrowserApi(
          '/browser/screenshot',
          'POST',
          { tabIndex, fullPage, quality, maxWidth, maxHeight },
          undefined,
          screenshotTimeout
        );

        const screenshot = typeof result.screenshot === 'string' ? result.screenshot : '';

        return {
          content: [{
            type: 'image',
            data: screenshot,
            mimeType: 'image/png'
          }],
          structuredContent: {
            screenshot
          }
        };
      } catch (error) {
        const message = error instanceof Error ? error.message : String(error);
        logger.error('Tool error: browser_screenshot', { error: message });
        return {
          content: [{
            type: 'text',
            text: `Failed to capture screenshot: ${message}`
          }],
          isError: true
        };
      }
    }
  );

  // ============================================================================
  // Tab Management Tools
  // ============================================================================

  server.registerTool(
    'window_create_tab',
    {
      title: 'Create Tab',
      description: 'Create a new browser tab',
      inputSchema: {
        url: z.string().url().describe('URL to load in the new tab')
      },
      outputSchema: {
        tabIndex: z.number(),
        url: z.string()
      }
    },
    async ({ url }) => {
      logger.info('Tool: window_create_tab', { url });
      try {
        const result = await callBrowserApi('/window/create', 'POST', { url });

        const output = {
          tabIndex: result.tabIndex ?? 0,
          url: url
        };

        return {
          content: [{
            type: 'text',
            text: `Created new tab at index ${output.tabIndex} with URL: ${output.url}`
          }],
          structuredContent: output
        };
      } catch (error) {
        const message = error instanceof Error ? error.message : String(error);
        logger.error('Tool error: window_create_tab', { error: message });
        return {
          content: [{
            type: 'text',
            text: `Failed to create tab: ${message}`
          }],
          isError: true
        };
      }
    }
  );

  server.registerTool(
    'window_close_tab',
    {
      title: 'Close Tab',
      description: 'Close a browser tab',
      inputSchema: {
        tabIndex: z.number().describe('Index of the tab to close')
      },
      outputSchema: {
        success: z.boolean(),
        tabIndex: z.number()
      }
    },
    async ({ tabIndex }) => {
      logger.info('Tool: window_close_tab', { tabIndex });
      try {
        await callBrowserApi('/window/close', 'POST', { tabIndex });

        const output = {
          success: true,
          tabIndex: tabIndex
        };

        return {
          content: [{
            type: 'text',
            text: `Closed tab at index ${tabIndex}`
          }],
          structuredContent: output
        };
      } catch (error) {
        const message = error instanceof Error ? error.message : String(error);
        logger.error('Tool error: window_close_tab', { error: message });
        return {
          content: [{
            type: 'text',
            text: `Failed to close tab: ${message}`
          }],
          isError: true
        };
      }
    }
  );

  server.registerTool(
    'window_switch_tab',
    {
      title: 'Switch Tab',
      description: 'Switch to a different browser tab',
      inputSchema: {
        tabIndex: z.number().describe('Index of the tab to switch to')
      },
      outputSchema: {
        success: z.boolean(),
        tabIndex: z.number()
      }
    },
    async ({ tabIndex }) => {
      logger.info('Tool: window_switch_tab', { tabIndex });
      try {
        await callBrowserApi('/window/switch', 'POST', { tabIndex });

        const output = {
          success: true,
          tabIndex: tabIndex
        };

        return {
          content: [{
            type: 'text',
            text: `Switched to tab ${tabIndex}`
          }],
          structuredContent: output
        };
      } catch (error) {
        const message = error instanceof Error ? error.message : String(error);
        logger.error('Tool error: window_switch_tab', { error: message });
        return {
          content: [{
            type: 'text',
            text: `Failed to switch tab: ${message}`
          }],
          isError: true
        };
      }
    }
  );

  server.registerTool(
    'window_get_tab_info',
    {
      title: 'Get Tab Info',
      description: 'Get information about all tabs (count and active tab index)',
      inputSchema: {},
      outputSchema: {
        count: z.number(),
        activeTabIndex: z.number()
      }
    },
    async () => {
      logger.info('Tool: window_get_tab_info');
      try {
        const result = await callBrowserApi('/window/info', 'GET');

        const output = {
          count: result.count ?? 0,
          activeTabIndex: result.activeTabIndex ?? 0
        };

        return {
          content: [{
            type: 'text',
            text: `Total tabs: ${output.count}, Active tab: ${output.activeTabIndex}`
          }],
          structuredContent: output
        };
      } catch (error) {
        const message = error instanceof Error ? error.message : String(error);
        logger.error('Tool error: window_get_tab_info', { error: message });
        return {
          content: [{
            type: 'text',
            text: `Failed to get tab info: ${message}`
          }],
          isError: true
        };
      }
    }
  );

  logger.info('Athena Browser MCP server created', {
    toolCount: 20
  });

  return server;
}
