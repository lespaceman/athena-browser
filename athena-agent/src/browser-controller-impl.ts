/**
 * Browser Controller Implementation
 *
 * This file provides a mock implementation of the BrowserController interface
 * for development and testing. In production, the C++ application will register
 * a real controller that calls into GtkWindow methods.
 *
 * To integrate with C++:
 * 1. The C++ code creates a callback mechanism
 * 2. The setBrowserController() is called with an object implementing BrowserController
 * 3. The routes in browser.ts call these methods
 * 4. The implementation uses NodeRuntime::Call() to send HTTP requests back to C++
 */

import type { BrowserController } from './routes/browser.js';
import { Logger } from './logger.js';

const logger = new Logger('BrowserControllerImpl');

/**
 * Simple in-memory mock implementation for testing.
 * Replace this with actual C++ bridge in production.
 */
export class MockBrowserController implements BrowserController {
  private tabs: Array<{ url: string; title: string }> = [];
  private activeTabIndex: number = 0;

  constructor() {
    // Start with one tab
    this.tabs.push({ url: 'about:blank', title: 'New Tab' });
    logger.info('Mock browser controller initialized');
  }

  async navigate(url: string, tabIndex?: number): Promise<void> {
    const idx = tabIndex ?? this.activeTabIndex;
    logger.info('Navigate', { url, tabIndex: idx });

    if (idx >= 0 && idx < this.tabs.length) {
      this.tabs[idx].url = url;
      this.tabs[idx].title = new URL(url).hostname;
    } else {
      throw new Error(`Invalid tab index: ${idx}`);
    }
  }

  async goBack(tabIndex?: number): Promise<void> {
    const idx = tabIndex ?? this.activeTabIndex;
    logger.info('Go back', { tabIndex: idx });
    // Mock: just log the action
  }

  async goForward(tabIndex?: number): Promise<void> {
    const idx = tabIndex ?? this.activeTabIndex;
    logger.info('Go forward', { tabIndex: idx });
    // Mock: just log the action
  }

  async reload(tabIndex?: number, ignoreCache?: boolean): Promise<void> {
    const idx = tabIndex ?? this.activeTabIndex;
    logger.info('Reload', { tabIndex: idx, ignoreCache });
    // Mock: just log the action
  }

  async getCurrentUrl(tabIndex?: number): Promise<string> {
    const idx = tabIndex ?? this.activeTabIndex;
    logger.info('Get current URL', { tabIndex: idx });

    if (idx >= 0 && idx < this.tabs.length) {
      return this.tabs[idx].url;
    }

    throw new Error(`Invalid tab index: ${idx}`);
  }

  async getPageHtml(tabIndex?: number): Promise<string> {
    const idx = tabIndex ?? this.activeTabIndex;
    logger.info('Get page HTML', { tabIndex: idx });

    // Mock: return simple HTML
    return `<html><body><h1>Mock Page</h1><p>URL: ${this.tabs[idx]?.url || 'unknown'}</p></body></html>`;
  }

  async executeJavaScript(code: string, tabIndex?: number): Promise<any> {
    const idx = tabIndex ?? this.activeTabIndex;
    logger.info('Execute JavaScript', { codeLength: code.length, tabIndex: idx });

    // Mock: return dummy result
    return { success: true, message: 'JavaScript execution mocked' };
  }

  async screenshot(tabIndex?: number, fullPage?: boolean): Promise<string> {
    const idx = tabIndex ?? this.activeTabIndex;
    logger.info('Screenshot', { tabIndex: idx, fullPage });

    // Mock: return dummy base64 data (1x1 transparent PNG)
    return 'iVBORw0KGgoAAAANSUhEUgAAAAEAAAABCAYAAAAfFcSJAAAADUlEQVR42mNk+M9QDwADhgGAWjR9awAAAABJRU5ErkJggg==';
  }

  async createTab(url: string): Promise<number> {
    logger.info('Create tab', { url });

    const newIndex = this.tabs.length;
    this.tabs.push({ url, title: new URL(url).hostname });
    this.activeTabIndex = newIndex;

    return newIndex;
  }

  async closeTab(tabIndex: number): Promise<void> {
    logger.info('Close tab', { tabIndex });

    if (tabIndex >= 0 && tabIndex < this.tabs.length) {
      this.tabs.splice(tabIndex, 1);

      // Adjust active tab if needed
      if (this.activeTabIndex >= this.tabs.length) {
        this.activeTabIndex = Math.max(0, this.tabs.length - 1);
      }
    } else {
      throw new Error(`Invalid tab index: ${tabIndex}`);
    }
  }

  async switchToTab(tabIndex: number): Promise<void> {
    logger.info('Switch to tab', { tabIndex });

    if (tabIndex >= 0 && tabIndex < this.tabs.length) {
      this.activeTabIndex = tabIndex;
    } else {
      throw new Error(`Invalid tab index: ${tabIndex}`);
    }
  }

  async getTabCount(): Promise<number> {
    logger.info('Get tab count');
    return this.tabs.length;
  }

  async getActiveTabIndex(): Promise<number> {
    logger.info('Get active tab index');
    return this.activeTabIndex;
  }

  /**
   * POC: Open URL and wait for load complete.
   * Simulates navigation with realistic timing.
   */
  async openUrl(url: string, timeoutMs: number = 10000): Promise<import('./routes/browser.js').OpenUrlResult> {
    const startTime = Date.now();

    logger.info('POC: Opening URL', { url, timeoutMs });

    try {
      // Validate URL
      const parsedUrl = new URL(url);
      if (parsedUrl.protocol !== 'https:' && parsedUrl.protocol !== 'http:') {
        return {
          success: false,
          error: `Invalid protocol: ${parsedUrl.protocol}. Only http: and https: are supported.`
        };
      }

      // Simulate load time (300-800ms)
      const loadTimeMs = Math.floor(Math.random() * 500) + 300;
      await new Promise(resolve => setTimeout(resolve, loadTimeMs));

      // Check if we exceeded timeout (shouldn't happen in mock, but for completeness)
      const elapsedMs = Date.now() - startTime;
      if (elapsedMs > timeoutMs) {
        return {
          success: false,
          error: `Navigation timed out after ${timeoutMs}ms`
        };
      }

      // Navigate active tab
      const idx = this.activeTabIndex;
      if (idx >= 0 && idx < this.tabs.length) {
        this.tabs[idx].url = url;
        this.tabs[idx].title = parsedUrl.hostname;
      }

      const actualLoadTime = Date.now() - startTime;

      logger.info('POC: URL opened successfully', {
        url,
        tabIndex: this.activeTabIndex,
        loadTimeMs: actualLoadTime
      });

      return {
        success: true,
        finalUrl: url,
        tabIndex: this.activeTabIndex,
        loadTimeMs: actualLoadTime
      };
    } catch (error) {
      logger.error('POC: Failed to open URL', {
        url,
        error: error instanceof Error ? error.message : String(error)
      });

      return {
        success: false,
        error: error instanceof Error ? error.message : 'Unknown error'
      };
    }
  }
}

/**
 * Create a mock browser controller for testing.
 * In production, this would be replaced by a C++ bridge.
 */
export function createMockBrowserController(): BrowserController {
  return new MockBrowserController();
}
