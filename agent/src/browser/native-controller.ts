/**
 * Native Browser Controller
 *
 * Implements BrowserController by making HTTP requests to the C++ browser's
 * internal control server over Unix socket.
 */

import type { BrowserController, OpenUrlResult } from './controller';
import type {
  BrowserApiResponse,
  PageSummary,
  InteractiveElement,
  AccessibilityNode,
  AnnotatedScreenshotElement
} from '../server/types';
import { Logger } from '../server/logger';
import { config } from '../server/config';
import * as http from 'http';

const logger = new Logger('NativeController');

// Response type interfaces for browser API calls
interface NavigateResponse extends BrowserApiResponse {
  finalUrl: string;
  tabIndex: number;
  loadTimeMs: number;
}

interface UrlResponse extends BrowserApiResponse {
  url: string;
}

interface HtmlResponse extends BrowserApiResponse {
  html: string;
}

interface ExecuteJsResponse extends BrowserApiResponse {
  result: unknown;
}

interface ScreenshotResponse extends BrowserApiResponse {
  screenshot: string;
}

interface TabResponse extends BrowserApiResponse {
  tabIndex: number;
}

interface TabInfoResponse extends BrowserApiResponse {
  count: number;
  activeTabIndex: number;
}

interface PageSummaryResponse extends BrowserApiResponse {
  summary: PageSummary;
}

interface InteractiveElementsResponse extends BrowserApiResponse {
  elements: InteractiveElement[];
}

interface AccessibilityTreeResponse extends BrowserApiResponse {
  tree: AccessibilityNode;
}

interface QueryContentResponse extends BrowserApiResponse {
  data: unknown;
}

interface AnnotatedScreenshotResponse extends BrowserApiResponse {
  screenshot: string;
  elements: AnnotatedScreenshotElement[];
}

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
    body?: string,
    timeoutMs: number = 30000 // Default 30s timeout for screenshot operations
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

      req.on('timeout', () => {
        req.destroy();
        reject(new Error(`Request timeout after ${timeoutMs}ms for ${path}`));
      });

      // Set timeout on the request
      req.setTimeout(timeoutMs);

      if (body) {
        req.write(body);
      }

      req.end();
    });
  }
}

/**
 * Native browser controller implementation.
 * Calls C++ QtMainWindow methods via HTTP over Unix socket.
 */
export class NativeBrowserController implements BrowserController {
  private client: UnixSocketHttpClient;

  private async requestJson<T = BrowserApiResponse>(
    method: string,
    path: string,
    body?: Record<string, unknown>,
    timeoutMs?: number
  ): Promise<T> {
    const payload = body
      ? JSON.stringify(
          Object.fromEntries(
            Object.entries(body).filter(([, value]) => value !== undefined)
          )
        )
      : undefined;

    const response = await this.client.request(method, path, payload, timeoutMs);
    let parsed: BrowserApiResponse = {};

    try {
      parsed = response.body ? JSON.parse(response.body) as BrowserApiResponse : {};
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

    return parsed as T;
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
      const result = await this.requestJson<NavigateResponse>('POST', '/internal/open_url', { url });
      const loadTimeMs =
        typeof result.loadTimeMs === 'number' ? result.loadTimeMs : Date.now() - startTime;

      logger.info('Native openUrl succeeded', {
        url: result.finalUrl,
        tabIndex: result.tabIndex,
        loadTimeMs
      });

      return {
        success: true,
        finalUrl: result.finalUrl,
        tabIndex: result.tabIndex,
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

    const result = await this.requestJson<NavigateResponse>('POST', '/internal/navigate', { url, tabIndex });
    logger.info('Native navigate completed', {
      url: result.finalUrl,
      tabIndex: result.tabIndex,
      loadTimeMs: result.loadTimeMs
    });
  }

  async goBack(tabIndex?: number): Promise<void> {
    const result = await this.requestJson<NavigateResponse>('POST', '/internal/history', {
      action: 'back',
      tabIndex
    });
    logger.info('Native back completed', {
      tabIndex: result.tabIndex,
      loadTimeMs: result.loadTimeMs,
      url: result.finalUrl
    });
  }

  async goForward(tabIndex?: number): Promise<void> {
    const result = await this.requestJson<NavigateResponse>('POST', '/internal/history', {
      action: 'forward',
      tabIndex
    });
    logger.info('Native forward completed', {
      tabIndex: result.tabIndex,
      loadTimeMs: result.loadTimeMs,
      url: result.finalUrl
    });
  }

  async reload(tabIndex?: number, ignoreCache?: boolean): Promise<void> {
    const result = await this.requestJson<NavigateResponse>('POST', '/internal/reload', {
      tabIndex,
      ignoreCache
    });
    logger.info('Native reload completed', {
      tabIndex: result.tabIndex,
      loadTimeMs: result.loadTimeMs,
      ignoreCache
    });
  }

  async getCurrentUrl(tabIndex?: number): Promise<string> {
    const result = await this.requestJson<UrlResponse>('POST', '/internal/get_url', { tabIndex });
    return result.url;
  }

  async getPageHtml(tabIndex?: number): Promise<string> {
    const result = await this.requestJson<HtmlResponse>('POST', '/internal/get_html', { tabIndex });
    return result.html;
  }

  async executeJavaScript(code: string, tabIndex?: number): Promise<unknown> {
    const result = await this.requestJson<ExecuteJsResponse>('POST', '/internal/execute_js', {
      code,
      tabIndex
    });
    return result.result;
  }

  async screenshot(tabIndex?: number, fullPage?: boolean, quality?: number, maxWidth?: number, maxHeight?: number): Promise<string> {
    const screenshotTimeout = config.screenshotTimeoutMs || 90000;
    const result = await this.requestJson<ScreenshotResponse>(
      'POST',
      '/internal/screenshot',
      { tabIndex, fullPage, quality, maxWidth, maxHeight },
      screenshotTimeout
    );
    return result.screenshot;
  }

  async createTab(url: string): Promise<number> {
    logger.info('Native createTab', { url });

    const result = await this.requestJson<TabResponse>('POST', '/internal/tab/create', { url });
    return result.tabIndex;
  }

  async closeTab(tabIndex: number): Promise<void> {
    await this.requestJson('POST', '/internal/tab/close', { tabIndex });
  }

  async switchToTab(tabIndex: number): Promise<void> {
    await this.requestJson('POST', '/internal/tab/switch', { tabIndex });
  }

  async getTabCount(): Promise<number> {
    const result = await this.requestJson<TabInfoResponse>('GET', '/internal/tab_info');
    return result.count;
  }

  async getActiveTabIndex(): Promise<number> {
    const result = await this.requestJson<TabInfoResponse>('GET', '/internal/tab_info');
    return result.activeTabIndex;
  }

  /**
   * Get a compact summary of the current page.
   * Returns title, headings, counts of interactive elements, etc.
   * Much smaller than full HTML (~1-2KB vs 100KB+).
   */
  async getPageSummary(tabIndex?: number): Promise<PageSummary> {
    const result = await this.requestJson<PageSummaryResponse>('POST', '/internal/get_page_summary', { tabIndex });
    return result.summary;
  }

  /**
   * Get list of interactive elements on the page with their positions and attributes.
   * Returns only visible, actionable elements (links, buttons, inputs, etc.).
   * Typical size: 5-20KB for complex pages.
   */
  async getInteractiveElements(tabIndex?: number): Promise<InteractiveElement[]> {
    const result = await this.requestJson<InteractiveElementsResponse>('POST', '/internal/get_interactive_elements', { tabIndex });

    // Validate response
    if (!result || typeof result !== 'object') {
      logger.error('getInteractiveElements: Invalid response structure', { result });
      throw new Error('Invalid response from browser');
    }

    if (!Array.isArray(result.elements)) {
      logger.error('getInteractiveElements: elements is not an array', {
        elementsType: typeof result.elements,
        elements: result.elements
      });
      return []; // Return empty array instead of throwing
    }

    return result.elements;
  }

  /**
   * Get accessibility tree representation of the page.
   * Provides semantic structure without full HTML.
   * Typical size: 10-30KB.
   */
  async getAccessibilityTree(tabIndex?: number): Promise<AccessibilityNode> {
    const result = await this.requestJson<AccessibilityTreeResponse>('POST', '/internal/get_accessibility_tree', { tabIndex });
    return result.tree;
  }

  /**
   * Query specific content types from the page.
   * Available types: 'forms', 'navigation', 'article', 'tables', 'media'
   * Returns only the requested content, much smaller than full HTML.
   */
  async queryContent(queryType: string, tabIndex?: number): Promise<unknown> {
    const result = await this.requestJson<QueryContentResponse>('POST', '/internal/query_content', {
      queryType,
      tabIndex
    });
    return result.data;
  }

  /**
   * Get screenshot with interactive element annotations.
   * Returns base64 screenshot + array of element positions.
   * Useful for vision-based interactions.
   */
  async getAnnotatedScreenshot(
    tabIndex?: number,
    quality?: number,
    maxWidth?: number,
    maxHeight?: number
  ): Promise<{ screenshot: string; elements: AnnotatedScreenshotElement[] }> {
    const screenshotTimeout = config.screenshotTimeoutMs || 90000;
    const result = await this.requestJson<AnnotatedScreenshotResponse>(
      'POST',
      '/internal/get_annotated_screenshot',
      { tabIndex, quality, maxWidth, maxHeight },
      screenshotTimeout
    );
    return {
      screenshot: result.screenshot,
      elements: result.elements
    };
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
