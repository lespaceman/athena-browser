/**
 * Browser API Client
 *
 * Single Responsibility: HTTP communication with the browser control API via Unix socket
 *
 * This module provides a clean interface for making HTTP requests to the browser
 * control server without any MCP-specific logic.
 */

import http from 'http';
import { URLSearchParams } from 'url';
import { Logger } from '../server/logger';

const logger = new Logger('BrowserApiClient');

export interface BrowserApiClientConfig {
  socketPath: string;
}

export class BrowserApiClient {
  private readonly socketPath: string;

  constructor(config: BrowserApiClientConfig) {
    this.socketPath = config.socketPath;
    logger.info('Browser API client initialized', { socketPath: this.socketPath });
  }

  /**
   * Make an HTTP request to the browser control API
   */
  async request<T = any>(
    endpoint: string,
    method: 'GET' | 'POST' = 'GET',
    body?: Record<string, any>,
    queryParams?: Record<string, string | number | boolean | undefined>,
    timeoutMs: number = 30000 // Default 30s timeout for screenshot operations
  ): Promise<T> {
    return new Promise((resolve, reject) => {
      // Use endpoint as-is (C++ server uses /internal/ prefix)
      const sanitizedEndpoint = endpoint.startsWith('/') ? endpoint : `/${endpoint}`;

      // Filter undefined values from body
      const filteredBody =
        body && typeof body === 'object'
          ? Object.fromEntries(
              Object.entries(body).filter(([, value]) => value !== undefined)
            )
          : undefined;

      const bodyStr = filteredBody ? JSON.stringify(filteredBody) : '';

      // Build query string
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

      // HTTP request options
      const options: http.RequestOptions = {
        socketPath: this.socketPath,
        path: sanitizedEndpoint + queryString,
        method,
        headers: {
          'Accept': 'application/json',
          'User-Agent': 'Athena-Agent/1.0',
          ...(bodyStr
            ? {
                'Content-Type': 'application/json',
                'Content-Length': Buffer.byteLength(bodyStr)
              }
            : {})
        }
      };

      logger.debug('Making browser API request', {
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
              // Check for logical errors in successful responses
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
              const message = result.error || result.message || 'Unknown error';
              logger.error('Browser API HTTP error', {
                endpoint: options.path,
                statusCode: res.statusCode,
                error: message
              });
              reject(new Error(message));
            }
          } catch (error) {
            logger.error('Failed to parse browser API response', {
              endpoint: options.path,
              error: error instanceof Error ? error.message : String(error),
              rawData: data.substring(0, 200)
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
   * Convenience methods for common operations
   * These use the /internal/ endpoints exposed by the C++ browser control server
   */

  async navigate(url: string, tabIndex?: number): Promise<any> {
    return this.request('/internal/navigate', 'POST', { url, tabIndex });
  }

  async getUrl(tabIndex?: number): Promise<any> {
    return this.request('/internal/get_url', 'GET', undefined, { tabIndex });
  }

  async getHtml(tabIndex?: number): Promise<any> {
    return this.request('/internal/get_html', 'GET', undefined, { tabIndex });
  }

  async executeJs(code: string, tabIndex?: number): Promise<any> {
    return this.request('/internal/execute_js', 'POST', { code, tabIndex });
  }

  async screenshot(tabIndex?: number, fullPage?: boolean): Promise<any> {
    return this.request('/internal/screenshot', 'POST', { tabIndex, fullPage });
  }

  async createTab(url: string): Promise<any> {
    return this.request('/internal/tab/create', 'POST', { url });
  }

  async closeTab(tabIndex: number): Promise<any> {
    return this.request('/internal/tab/close', 'POST', { tabIndex });
  }

  async switchTab(tabIndex: number): Promise<any> {
    return this.request('/internal/tab/switch', 'POST', { tabIndex });
  }

  async getTabInfo(): Promise<any> {
    return this.request('/internal/tab_info', 'GET');
  }
}
