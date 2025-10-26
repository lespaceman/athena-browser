/**
 * Browser Control Interface
 *
 * Defines the interface for browser control operations.
 * This is implemented by NativeController (real) and MockController (testing).
 */

import { Logger } from '../server/logger';

const logger = new Logger('BrowserController');

// ============================================================================
// Types
// ============================================================================

/**
 * Result of opening a URL (POC endpoint)
 */
export interface OpenUrlResult {
  success: boolean;
  finalUrl?: string;
  tabIndex?: number;
  loadTimeMs?: number;
  error?: string;
}

/**
 * Interface for browser control operations.
 * This will be implemented by a bridge to the C++ QtMainWindow API.
 */
export interface BrowserController {
  // Navigation
  navigate(url: string, tabIndex?: number): Promise<void>;
  goBack(tabIndex?: number): Promise<void>;
  goForward(tabIndex?: number): Promise<void>;
  reload(tabIndex?: number, ignoreCache?: boolean): Promise<void>;

  // Information
  getCurrentUrl(tabIndex?: number): Promise<string>;
  getPageHtml(tabIndex?: number): Promise<string>;

  // Interaction
  executeJavaScript(code: string, tabIndex?: number): Promise<unknown>;
  screenshot(tabIndex?: number, fullPage?: boolean, quality?: number, maxWidth?: number, maxHeight?: number): Promise<string>; // Returns base64

  // Tab management
  createTab(url: string): Promise<number>; // Returns tab index
  closeTab(tabIndex: number): Promise<void>;
  switchToTab(tabIndex: number): Promise<void>;
  getTabCount(): Promise<number>;
  getActiveTabIndex(): Promise<number>;

  // POC: Combined operation
  /**
   * Open URL and wait for load complete (POC endpoint).
   * Creates/reuses tab, navigates, and waits for load confirmation.
   * @param url URL to open
   * @param timeoutMs Timeout in milliseconds (default: 10000)
   * @returns Result with success status and load metrics
   */
  openUrl(url: string, timeoutMs?: number): Promise<OpenUrlResult>;
}

// ============================================================================
// Controller Registry
// ============================================================================

/**
 * Global browser controller instance.
 * This is set by the C++ application when it creates the window.
 */
let browserController: BrowserController | null = null;

/**
 * Set the browser controller instance.
 * Called by C++ during window initialization.
 */
export function setBrowserController(controller: BrowserController) {
  browserController = controller;
  logger.info('Browser controller registered');
}

/**
 * Get the browser controller instance.
 * Throws if not initialized.
 */
export function getController(): BrowserController {
  if (!browserController) {
    throw new Error('Browser controller not initialized. Window may not be ready yet.');
  }
  return browserController;
}
