/**
 * Native Browser Controller
 *
 * Implements BrowserController by making HTTP requests to the C++ browser's
 * internal control server over Unix socket.
 */

import type { BrowserController, OpenUrlResult } from './routes/browser.js';
import { Logger } from './logger.js';
import * as http from 'http';

const logger = new Logger('NativeController');

/**
 * HTTP client for Unix socket communication
 */
class UnixSocketHttpClient {
  constructor(private socketPath: string) {}

  /**
   * Make an HTTP request over Unix socket
   */
  async request(
    method: string,
    path: string,
    body?: string
  ): Promise<{ statusCode: number; body: string }> {
    return new Promise((resolve, reject) => {
      const options: http.RequestOptions = {
        socketPath: this.socketPath,
        method,
        path,
        headers: {
          'Content-Type': 'application/json',
          ...(body ? { 'Content-Length': Buffer.byteLength(body) } : {})
        }
      };

      const req = http.request(options, res => {
        let data = '';

        res.on('data', chunk => {
          data += chunk;
        });

        res.on('end', () => {
          resolve({
            statusCode: res.statusCode || 500,
            body: data
          });
        });
      });

      req.on('error', err => {
        reject(err);
      });

      if (body) {
        req.write(body);
      }

      req.end();
    });
  }
}

/**
 * Native browser controller implementation.
 * Calls C++ GtkWindow methods via HTTP over Unix socket.
 */
export class NativeBrowserController implements BrowserController {
  private client: UnixSocketHttpClient;

  constructor() {
    // Unix socket path for browser control server
    const uid = process.getuid?.() ?? 1000;
    const socketPath = `/tmp/athena-${uid}-control.sock`;

    this.client = new UnixSocketHttpClient(socketPath);

    logger.info('Native controller initialized', { socketPath });
  }

  /**
   * POC: Open URL and wait for load complete.
   */
  async openUrl(url: string, timeoutMs: number = 10000): Promise<OpenUrlResult> {
    const startTime = Date.now();

    logger.info('Native openUrl', { url, timeoutMs });

    try {
      // Make HTTP POST to C++ internal server
      const response = await this.client.request(
        'POST',
        '/internal/open_url',
        JSON.stringify({ url })
      );

      const loadTimeMs = Date.now() - startTime;

      // Parse response
      const result = JSON.parse(response.body);

      if (result.success) {
        logger.info('Native openUrl succeeded', {
          url,
          tabIndex: result.tabIndex,
          loadTimeMs
        });

        return {
          success: true,
          finalUrl: url,
          tabIndex: result.tabIndex ?? 0,
          loadTimeMs
        };
      } else {
        logger.error('Native openUrl failed', { error: result.error });

        return {
          success: false,
          error: result.error || 'Unknown error from browser'
        };
      }
    } catch (error) {
      logger.error('Exception in native openUrl', {
        error: error instanceof Error ? error.message : String(error)
      });

      return {
        success: false,
        error: error instanceof Error ? error.message : 'Failed to connect to browser'
      };
    }
  }

  // Implement other required methods (delegate to native or throw not implemented)

  async navigate(url: string, tabIndex?: number): Promise<void> {
    logger.info('Native navigate', { url, tabIndex });

    // For now, just use openUrl
    const result = await this.openUrl(url);
    if (!result.success) {
      throw new Error(result.error || 'Navigation failed');
    }
  }

  async goBack(_tabIndex?: number): Promise<void> {
    logger.warn('goBack not implemented in native module yet');
    throw new Error('Not implemented');
  }

  async goForward(_tabIndex?: number): Promise<void> {
    logger.warn('goForward not implemented in native module yet');
    throw new Error('Not implemented');
  }

  async reload(_tabIndex?: number, _ignoreCache?: boolean): Promise<void> {
    logger.warn('reload not implemented in native module yet');
    throw new Error('Not implemented');
  }

  async getCurrentUrl(_tabIndex?: number): Promise<string> {
    try {
      const response = await this.client.request('GET', '/internal/get_url');
      const result = JSON.parse(response.body);

      if (result.success) {
        return result.url;
      } else {
        throw new Error(result.error || 'Failed to get URL');
      }
    } catch (error) {
      throw new Error(
        error instanceof Error ? error.message : 'Failed to get URL'
      );
    }
  }

  async getPageHtml(_tabIndex?: number): Promise<string> {
    try {
      const response = await this.client.request('GET', '/internal/get_html');
      const result = JSON.parse(response.body);

      if (result.success) {
        return result.html;
      } else {
        throw new Error(result.error || 'Failed to get HTML');
      }
    } catch (error) {
      throw new Error(
        error instanceof Error ? error.message : 'Failed to get HTML'
      );
    }
  }

  async executeJavaScript(code: string, _tabIndex?: number): Promise<any> {
    try {
      const response = await this.client.request(
        'POST',
        '/internal/execute_js',
        JSON.stringify({ code })
      );
      const result = JSON.parse(response.body);

      if (result.success) {
        return result.result;
      } else {
        throw new Error(result.error || 'Failed to execute JavaScript');
      }
    } catch (error) {
      throw new Error(
        error instanceof Error ? error.message : 'Failed to execute JavaScript'
      );
    }
  }

  async screenshot(_tabIndex?: number, _fullPage?: boolean): Promise<string> {
    try {
      const response = await this.client.request('GET', '/internal/screenshot');
      const result = JSON.parse(response.body);

      if (result.success) {
        return result.screenshot;
      } else {
        throw new Error(result.error || 'Failed to take screenshot');
      }
    } catch (error) {
      throw new Error(
        error instanceof Error ? error.message : 'Failed to take screenshot'
      );
    }
  }

  async createTab(url: string): Promise<number> {
    logger.info('Native createTab', { url });

    const result = await this.openUrl(url);
    if (!result.success) {
      throw new Error(result.error || 'Tab creation failed');
    }

    return result.tabIndex ?? 0;
  }

  async closeTab(_tabIndex: number): Promise<void> {
    logger.warn('closeTab not implemented in native module yet');
    throw new Error('Not implemented');
  }

  async switchToTab(_tabIndex: number): Promise<void> {
    logger.warn('switchToTab not implemented in native module yet');
    throw new Error('Not implemented');
  }

  async getTabCount(): Promise<number> {
    try {
      const response = await this.client.request('GET', '/internal/tab_count');
      const result = JSON.parse(response.body);

      if (result.success) {
        return result.count;
      } else {
        return 0;
      }
    } catch (error) {
      return 0;
    }
  }

  async getActiveTabIndex(): Promise<number> {
    // Not available from native yet, return 0
    return 0;
  }
}

/**
 * Create native browser controller.
 * Falls back to null if native module cannot be loaded.
 */
export function createNativeBrowserController(): BrowserController | null {
  try {
    return new NativeBrowserController();
  } catch (error) {
    logger.error('Failed to create native controller', {
      error: error instanceof Error ? error.message : String(error)
    });
    return null;
  }
}
