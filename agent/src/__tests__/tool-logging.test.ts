import { describe, expect, it } from 'vitest';
import { summarizeToolResult, summarizeToolError } from '../utils/tool-logging';

describe('tool logging helpers', () => {
  it('summarizes successful HTML response without leaking contents', () => {
    const result = summarizeToolResult('browser_get_html', {
      success: true,
      html: '<html><body>Hello</body></html>',
      tabIndex: 0
    });

    expect(result.tool).toBe('browser_get_html');
    expect(result.success).toBe(true);
    expect(result.htmlLength).toBeGreaterThan(0);
    expect(result.keys).toContain('html');
    expect(result).not.toHaveProperty('html');
  });

  it('summarizes screenshot payloads by size only', () => {
    const fakeScreenshot = Buffer.from('image-bytes').toString('base64');
    const result = summarizeToolResult('browser_screenshot', {
      success: true,
      screenshot: fakeScreenshot
    });

    expect(result.screenshotBytes).toBeGreaterThan(0);
    expect(result.keys).toContain('screenshot');
  });

  it('summarizes errors with stack traces', () => {
    const error = new Error('Boom');
    const summary = summarizeToolError('browser_get_html', error);

    expect(summary.tool).toBe('browser_get_html');
    expect(summary.message).toBe('Boom');
    expect(typeof summary.stack === 'string' || summary.stack === undefined).toBeTruthy();
  });

  it('handles primitive results gracefully', () => {
    const summary = summarizeToolResult('browser_execute_js', 'ok');
    expect(summary.resultType).toBe('string');
    expect(summary.preview?.startsWith('ok')).toBe(true);
  });
});
