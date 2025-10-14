/**
 * Integration Test: Browser Control via HTTP
 *
 * Tests the full stack from HTTP endpoints → BrowserController
 * Uses the mock controller to verify the plumbing works correctly.
 */

import { describe, it, expect, beforeAll, afterAll } from 'vitest';
import http from 'http';
import { unlinkSync, existsSync } from 'fs';

const TEST_SOCKET = '/tmp/athena-test-integration.sock';

interface TestResponse {
  statusCode: number;
  body: any;
}

/**
 * Make an HTTP request to the test socket
 */
async function request(method: string, path: string, body?: any): Promise<TestResponse> {
  return new Promise((resolve, reject) => {
    const bodyStr = body ? JSON.stringify(body) : '';

    const options: http.RequestOptions = {
      socketPath: TEST_SOCKET,
      path,
      method,
      headers: {
        'Content-Type': 'application/json',
        'Content-Length': Buffer.byteLength(bodyStr)
      }
    };

    const req = http.request(options, (res) => {
      let data = '';
      res.on('data', chunk => data += chunk);
      res.on('end', () => {
        try {
          const result: TestResponse = {
            statusCode: res.statusCode || 500,
            body: JSON.parse(data)
          };
          resolve(result);
        } catch (error) {
          reject(new Error(`Failed to parse response: ${data}`));
        }
      });
    });

    req.on('error', reject);

    if (bodyStr) {
      req.write(bodyStr);
    }

    req.end();
  });
}

describe('Browser Control Integration', () => {
  let serverProcess: any;

  beforeAll(async () => {
    // Clean up any existing socket
    if (existsSync(TEST_SOCKET)) {
      unlinkSync(TEST_SOCKET);
    }

    // Start the server
    const { spawn } = await import('child_process');
    serverProcess = spawn('node', ['dist/server.js'], {
      env: {
        ...process.env,
        ATHENA_SOCKET_PATH: TEST_SOCKET
      },
      stdio: ['ignore', 'pipe', 'pipe']
    });

    // Wait for server to be ready
    await new Promise<void>((resolve, reject) => {
      const timeout = setTimeout(() => {
        reject(new Error('Server startup timeout'));
      }, 10000);

      serverProcess.stdout.on('data', (data: Buffer) => {
        if (data.toString().includes('READY')) {
          clearTimeout(timeout);
          resolve();
        }
      });

      serverProcess.on('error', (err: Error) => {
        clearTimeout(timeout);
        reject(err);
      });
    });

    // Give server a moment to fully initialize
    await new Promise(resolve => setTimeout(resolve, 500));
  });

  afterAll(() => {
    if (serverProcess) {
      serverProcess.kill();
    }
    if (existsSync(TEST_SOCKET)) {
      unlinkSync(TEST_SOCKET);
    }
  });

  describe('Health Check', () => {
    it('should return healthy status', async () => {
      const res = await request('GET', '/health');

      expect(res.statusCode).toBe(200);
      expect(res.body.status).toBe('healthy');
      expect(res.body.ready).toBe(true);
    });
  });

  describe('Tab Management', () => {
    it('should get initial tab info', async () => {
      const res = await request('GET', '/v1/window/tab_info');

      expect(res.statusCode).toBe(200);
      expect(res.body.success).toBe(true);
      expect(res.body.tabCount).toBe(1); // Mock starts with 1 tab
      expect(res.body.activeTabIndex).toBe(0);
    });

    it('should create a new tab', async () => {
      const res = await request('POST', '/v1/window/create_tab', {
        url: 'https://example.com'
      });

      expect(res.statusCode).toBe(200);
      expect(res.body.success).toBe(true);
      expect(res.body.tabIndex).toBe(1);
    });

    it('should reflect new tab in tab info', async () => {
      const res = await request('GET', '/v1/window/tab_info');

      expect(res.statusCode).toBe(200);
      expect(res.body.tabCount).toBe(2);
      expect(res.body.activeTabIndex).toBe(1); // Mock switches to new tab
    });

    it('should switch tabs', async () => {
      const res = await request('POST', '/v1/window/switch_tab', {
        tabIndex: 0
      });

      expect(res.statusCode).toBe(200);
      expect(res.body.success).toBe(true);

      const info = await request('GET', '/v1/window/tab_info');
      expect(info.body.activeTabIndex).toBe(0);
    });

    it('should close a tab', async () => {
      const res = await request('POST', '/v1/window/close_tab', {
        tabIndex: 0
      });

      expect(res.statusCode).toBe(200);
      expect(res.body.success).toBe(true);

      const info = await request('GET', '/v1/window/tab_info');
      expect(info.body.tabCount).toBe(1);
    });
  });

  describe('Navigation', () => {
    it('should navigate to a URL', async () => {
      const res = await request('POST', '/v1/browser/navigate', {
        url: 'https://github.com'
      });

      expect(res.statusCode).toBe(200);
      expect(res.body.success).toBe(true);
    });

    it('should get current URL', async () => {
      const res = await request('GET', '/v1/browser/get_url');

      expect(res.statusCode).toBe(200);
      expect(res.body.success).toBe(true);
      expect(res.body.url).toBe('https://github.com');
    });

    it('should reload page', async () => {
      const res = await request('POST', '/v1/browser/reload', {
        ignoreCache: false
      });

      expect(res.statusCode).toBe(200);
      expect(res.body.success).toBe(true);
    });

    it('should go back in history', async () => {
      const res = await request('POST', '/v1/browser/back');

      expect(res.statusCode).toBe(200);
      expect(res.body.success).toBe(true);
    });

    it('should go forward in history', async () => {
      const res = await request('POST', '/v1/browser/forward');

      expect(res.statusCode).toBe(200);
      expect(res.body.success).toBe(true);
    });
  });

  describe('Page Interaction', () => {
    it('should get page HTML', async () => {
      const res = await request('GET', '/v1/browser/get_html');

      expect(res.statusCode).toBe(200);
      expect(res.body.success).toBe(true);
      expect(res.body.html).toBeDefined();
      expect(typeof res.body.html).toBe('string');
    });

    it('should execute JavaScript', async () => {
      const res = await request('POST', '/v1/browser/execute_js', {
        code: 'document.title'
      });

      expect(res.statusCode).toBe(200);
      expect(res.body.success).toBe(true);
      expect(res.body.result).toBeDefined();
    });

    it('should capture screenshot', async () => {
      const res = await request('POST', '/v1/browser/screenshot', {
        fullPage: false
      });

      expect(res.statusCode).toBe(200);
      expect(res.body.success).toBe(true);
      expect(res.body.imageData).toBeDefined();
      expect(typeof res.body.imageData).toBe('string');
    });
  });

  describe('Error Handling', () => {
    it('should return 400 for invalid URL in navigate', async () => {
      const res = await request('POST', '/v1/browser/navigate', {
        url: 123 // Invalid type
      });

      expect(res.statusCode).toBe(400);
      expect(res.body.error).toBe('Bad Request');
    });

    it('should return 400 for missing URL in create_tab', async () => {
      const res = await request('POST', '/v1/window/create_tab', {});

      expect(res.statusCode).toBe(400);
      expect(res.body.error).toBe('Bad Request');
    });

    it('should return 400 for invalid tabIndex in close_tab', async () => {
      const res = await request('POST', '/v1/window/close_tab', {
        tabIndex: 'invalid'
      });

      expect(res.statusCode).toBe(400);
      expect(res.body.error).toBe('Bad Request');
    });

    it('should return 404 for non-existent endpoint', async () => {
      const res = await request('GET', '/v1/nonexistent');

      expect(res.statusCode).toBe(404);
      expect(res.body.error).toBe('Not Found');
    });
  });

  describe('Capabilities', () => {
    it('should return server capabilities', async () => {
      const res = await request('GET', '/v1/capabilities');

      expect(res.statusCode).toBe(200);
      expect(res.body.features).toBeDefined();
      expect(Array.isArray(res.body.features)).toBe(true);
    });
  });
});
