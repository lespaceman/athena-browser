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

  private async requestJson(
    method: string,
    path: string,
    body?: Record<string, any>
  ): Promise<any> {
    const payload = body
      ? JSON.stringify(
          Object.fromEntries(
            Object.entries(body).filter(([, value]) => value !== undefined)
          )
        )
      : undefined;

    const response = await this.client.request(method, path, payload);
    let parsed: any = {};

    try {
      parsed = response.body ? JSON.parse(response.body) : {};
    } catch (error) {
      throw new Error(
        `Failed to parse response from ${path}: ${
          error instanceof Error ? error.message : String(error)
        }`
      );
    }

    if (response.statusCode < 200 || response.statusCode >= 300 || parsed.success === false) {
      const message =
        parsed.error ||
        parsed.message ||
        `Request to ${path} failed with status ${response.statusCode}`;
      throw new Error(message);
    }

    return parsed;
  }

  constructor() {
    // Unix socket path for C++ browser control server
    // This is passed via ATHENA_CONTROL_SOCKET_PATH environment variable by the C++ runtime
    const uid = process.getuid?.() ?? 1000;
    const socketPath = process.env.ATHENA_CONTROL_SOCKET_PATH || `/tmp/athena-${uid}-control.sock`;

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
      const result = await this.requestJson('POST', '/internal/open_url', { url });
      const loadTimeMs =
        typeof result.loadTimeMs === 'number' ? result.loadTimeMs : Date.now() - startTime;

      logger.info('Native openUrl succeeded', {
        url: result.finalUrl ?? url,
        tabIndex: result.tabIndex,
        loadTimeMs
      });

      return {
        success: true,
        finalUrl: result.finalUrl ?? url,
        tabIndex: result.tabIndex ?? 0,
        loadTimeMs
      };
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

    const result = await this.requestJson('POST', '/internal/navigate', { url, tabIndex });
    logger.info('Native navigate completed', {
      url: result.finalUrl ?? url,
      tabIndex: result.tabIndex ?? tabIndex ?? 0,
      loadTimeMs: result.loadTimeMs
    });
  }

  async goBack(tabIndex?: number): Promise<void> {
    const result = await this.requestJson('POST', '/internal/history', {
      action: 'back',
      tabIndex
    });
    logger.info('Native back completed', {
      tabIndex: result.tabIndex ?? tabIndex ?? 0,
      loadTimeMs: result.loadTimeMs,
      url: result.finalUrl
    });
  }

  async goForward(tabIndex?: number): Promise<void> {
    const result = await this.requestJson('POST', '/internal/history', {
      action: 'forward',
      tabIndex
    });
    logger.info('Native forward completed', {
      tabIndex: result.tabIndex ?? tabIndex ?? 0,
      loadTimeMs: result.loadTimeMs,
      url: result.finalUrl
    });
  }

  async reload(tabIndex?: number, ignoreCache?: boolean): Promise<void> {
    const result = await this.requestJson('POST', '/internal/reload', {
      tabIndex,
      ignoreCache
    });
    logger.info('Native reload completed', {
      tabIndex: result.tabIndex ?? tabIndex ?? 0,
      loadTimeMs: result.loadTimeMs,
      ignoreCache: result.ignoreCache
    });
  }

  async getCurrentUrl(tabIndex?: number): Promise<string> {
    const result = await this.requestJson('POST', '/internal/get_url', { tabIndex });
    return result.url;
  }

  async getPageHtml(tabIndex?: number): Promise<string> {
    const result = await this.requestJson('POST', '/internal/get_html', { tabIndex });
    return result.html;
  }

  async executeJavaScript(code: string, tabIndex?: number): Promise<any> {
    const result = await this.requestJson('POST', '/internal/execute_js', {
      code,
      tabIndex
    });
    return result.result;
  }

  async screenshot(tabIndex?: number, fullPage?: boolean): Promise<string> {
    const result = await this.requestJson('POST', '/internal/screenshot', {
      tabIndex,
      fullPage
    });
    return result.screenshot;
  }

  async createTab(url: string): Promise<number> {
    logger.info('Native createTab', { url });

    const result = await this.requestJson('POST', '/internal/tab/create', { url });
    return result.tabIndex;
  }

  async closeTab(tabIndex: number): Promise<void> {
    await this.requestJson('POST', '/internal/tab/close', { tabIndex });
  }

  async switchToTab(tabIndex: number): Promise<void> {
    await this.requestJson('POST', '/internal/tab/switch', { tabIndex });
  }

  async getTabCount(): Promise<number> {
    const result = await this.requestJson('GET', '/internal/tab_info');
    return result.count ?? 0;
  }

  async getActiveTabIndex(): Promise<number> {
    const result = await this.requestJson('GET', '/internal/tab_info');
    return result.activeTabIndex ?? 0;
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
