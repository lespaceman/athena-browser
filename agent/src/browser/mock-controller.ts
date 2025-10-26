/**
 * Browser Controller Implementation
 *
 * This file provides a mock implementation of the BrowserController interface
 * for development and testing. In production, the C++ application will register
 * a real controller that calls into QtMainWindow methods.
 *
 * To integrate with C++:
 * 1. The C++ code creates a callback mechanism
 * 2. The setBrowserController() is called with an object implementing BrowserController
 * 3. The routes in browser.ts call these methods
 * 4. The implementation uses NodeRuntime::Call() to send HTTP requests back to C++
 */

import type { BrowserController } from './controller';
import type {
  PageSummary,
  InteractiveElement,
  AccessibilityNode,
  AnnotatedScreenshotElement
} from '../server/types';
import { Logger } from '../server/logger';

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

  async executeJavaScript(code: string, tabIndex?: number): Promise<unknown> {
    const idx = tabIndex ?? this.activeTabIndex;
    logger.info('Execute JavaScript', { codeLength: code.length, tabIndex: idx });

    // Mock: return dummy result
    return { success: true, message: 'JavaScript execution mocked' };
  }

  async screenshot(tabIndex?: number, fullPage?: boolean, quality?: number, maxWidth?: number, maxHeight?: number): Promise<string> {
    const idx = tabIndex ?? this.activeTabIndex;
    logger.info('Screenshot', { tabIndex: idx, fullPage, quality, maxWidth, maxHeight });

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
   * Get a compact summary of the current page.
   */
  async getPageSummary(tabIndex?: number): Promise<PageSummary> {
    const idx = tabIndex ?? this.activeTabIndex;
    logger.info('Get page summary', { tabIndex: idx });

    return {
      title: 'Mock Page',
      url: this.tabs[idx]?.url || 'about:blank',
      headings: ['Mock Heading 1', 'Mock Heading 2'],
      forms: 1,
      links: 5,
      buttons: 3,
      inputs: 2,
      images: 4,
      mainText: 'This is a mock page summary with some sample text content.'
    };
  }

  /**
   * Get list of interactive elements on the page.
   */
  async getInteractiveElements(tabIndex?: number): Promise<InteractiveElement[]> {
    const idx = tabIndex ?? this.activeTabIndex;
    logger.info('Get interactive elements', { tabIndex: idx });

    return [
      { index: 0, tag: 'button', type: '', text: 'Click me', bounds: { x: 10, y: 10, width: 100, height: 40 } },
      { index: 1, tag: 'a', type: '', text: 'Link 1', href: '#link1', bounds: { x: 10, y: 60, width: 80, height: 20 } },
      { index: 2, tag: 'input', type: 'text', placeholder: 'Enter text', bounds: { x: 10, y: 90, width: 200, height: 30 } }
    ];
  }

  /**
   * Get accessibility tree representation of the page.
   */
  async getAccessibilityTree(tabIndex?: number): Promise<AccessibilityNode> {
    const idx = tabIndex ?? this.activeTabIndex;
    logger.info('Get accessibility tree', { tabIndex: idx });

    return {
      role: 'WebArea',
      name: 'Mock Page',
      children: [
        { role: 'heading', name: 'Mock Heading 1', level: 1 },
        { role: 'button', name: 'Click me' },
        { role: 'link', name: 'Link 1' }
      ]
    };
  }

  /**
   * Query specific content types from the page.
   */
  async queryContent(queryType: string, tabIndex?: number): Promise<unknown> {
    const idx = tabIndex ?? this.activeTabIndex;
    logger.info('Query content', { queryType, tabIndex: idx });

    const mockData: Record<string, unknown> = {
      forms: [{ id: 'mock-form', fields: [{ name: 'email', type: 'email' }] }],
      navigation: [{ text: 'Home', href: '/' }, { text: 'About', href: '/about' }],
      article: { title: 'Mock Article', content: 'Mock article content here.' },
      tables: [{ headers: ['Col1', 'Col2'], rows: [[' Cell1', 'Cell2']] }],
      media: [{ type: 'image', src: '/mock.png', alt: 'Mock image' }]
    };

    return mockData[queryType] || {};
  }

  /**
   * Get screenshot with interactive element annotations.
   */
  async getAnnotatedScreenshot(
    tabIndex?: number,
    quality?: number,
    maxWidth?: number,
    maxHeight?: number
  ): Promise<{ screenshot: string; elements: AnnotatedScreenshotElement[] }> {
    const idx = tabIndex ?? this.activeTabIndex;
    logger.info('Get annotated screenshot', { tabIndex: idx, quality, maxWidth, maxHeight });

    // Return mock base64 image (1x1 transparent PNG) + mock elements
    return {
      screenshot: 'iVBORw0KGgoAAAANSUhEUgAAAAEAAAABCAYAAAAfFcSJAAAADUlEQVR42mNk+M9QDwADhgGAWjR9awAAAABJRU5ErkJggg==',
      elements: [
        { index: 0, x: 10, y: 10, width: 100, height: 40, tag: 'button', text: 'Click me', type: '' },
        { index: 1, x: 10, y: 60, width: 80, height: 20, tag: 'a', text: 'Link 1', type: '' }
      ]
    };
  }

  /**
   * POC: Open URL and wait for load complete.
   * Simulates navigation with realistic timing.
   */
  async openUrl(url: string, timeoutMs: number = 10000): Promise<import('./controller').OpenUrlResult> {
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
