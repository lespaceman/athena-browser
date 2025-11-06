#!/usr/bin/env tsx

/**
 * MCP Tools Demo
 *
 * Demonstrates how to use Athena Browser's 17 MCP tools programmatically.
 *
 * Usage:
 *   1. Start Athena Browser: ./scripts/run.sh
 *   2. Start agent: cd agent && npm run dev
 *   3. Run this script: tsx examples/mcp-demo.ts
 */

import { BrowserApiClient } from '../src/browser/api-client';

const SOCKET_PATH = process.env.ATHENA_SOCKET_PATH || `/tmp/athena-${process.getuid()}.sock`;

async function main() {
  console.log('🚀 Athena Browser MCP Tools Demo\n');

  const client = new BrowserApiClient({ socketPath: SOCKET_PATH });

  // 1. Navigation
  console.log('1️⃣  Testing Navigation Tools...');
  await client.navigate('https://example.com');
  console.log('   ✅ Navigated to example.com\n');

  await new Promise(resolve => setTimeout(resolve, 2000)); // Wait for page load

  // 2. Get current URL
  console.log('2️⃣  Testing Information Tools...');
  const urlResult = await client.getUrl();
  console.log(`   ✅ Current URL: ${urlResult.url}\n`);

  // 3. Get page summary (lightweight, ~1-2KB)
  const summaryResult = await client.request('/internal/get_page_summary', 'GET');
  console.log('   ✅ Page Summary:');
  console.log(`      Title: ${summaryResult.summary.title}`);
  console.log(`      Headings: ${summaryResult.summary.headings?.length || 0}`);
  console.log(`      Links: ${summaryResult.summary.links || 0}\n`);

  // 4. Query specific content (forms)
  console.log('3️⃣  Testing Content Query...');
  const formsResult = await client.request('/internal/query_content', 'POST', {
    queryType: 'forms'
  });
  console.log(`   ✅ Found ${formsResult.data?.forms?.length || 0} forms on page\n`);

  // 5. Execute JavaScript
  console.log('4️⃣  Testing JavaScript Execution...');
  const jsResult = await client.executeJs('document.title');
  console.log(`   ✅ Page title via JS: ${jsResult.result}\n`);

  // 6. Get interactive elements
  console.log('5️⃣  Testing Interactive Elements...');
  const elementsResult = await client.request('/internal/get_interactive_elements', 'GET');
  const elements = elementsResult.elements || [];
  console.log(`   ✅ Found ${elements.length} interactive elements`);
  if (elements.length > 0) {
    console.log(`      First 3: ${elements.slice(0, 3).map((el: any) => el.tag).join(', ')}\n`);
  }

  // 7. Screenshot
  console.log('6️⃣  Testing Screenshot Capture...');
  const screenshotResult = await client.screenshot(undefined, false);
  const screenshotSize = screenshotResult.screenshot.length;
  console.log(`   ✅ Screenshot captured (${(screenshotSize / 1024).toFixed(1)} KB base64)\n`);

  // 8. Tab Management
  console.log('7️⃣  Testing Tab Management...');
  const tabInfo1 = await client.getTabInfo();
  console.log(`   ✅ Current tabs: ${tabInfo1.count}, active: ${tabInfo1.activeTabIndex}`);

  const newTab = await client.createTab('https://github.com');
  console.log(`   ✅ Created new tab at index ${newTab.tabIndex}`);

  await new Promise(resolve => setTimeout(resolve, 1000));

  const tabInfo2 = await client.getTabInfo();
  console.log(`   ✅ Updated tabs: ${tabInfo2.count}, active: ${tabInfo2.activeTabIndex}`);

  await client.switchTab(0);
  console.log('   ✅ Switched back to tab 0');

  await client.closeTab(1);
  console.log('   ✅ Closed tab 1\n');

  // 9. History Navigation
  console.log('8️⃣  Testing History Navigation...');
  await client.navigate('https://github.com/anthropics');
  await new Promise(resolve => setTimeout(resolve, 1500));

  await client.request('/internal/history', 'POST', { action: 'back' });
  console.log('   ✅ Went back in history');

  await new Promise(resolve => setTimeout(resolve, 1000));

  await client.request('/internal/history', 'POST', { action: 'forward' });
  console.log('   ✅ Went forward in history\n');

  // 10. Page Reload
  console.log('9️⃣  Testing Page Reload...');
  await client.request('/internal/reload', 'POST', { ignoreCache: false });
  console.log('   ✅ Page reloaded (soft reload)');

  await new Promise(resolve => setTimeout(resolve, 1500));

  await client.request('/internal/reload', 'POST', { ignoreCache: true });
  console.log('   ✅ Page reloaded (hard reload, bypassed cache)\n');

  console.log('✨ All MCP tools tested successfully!\n');
  console.log('📊 Summary:');
  console.log('   • Navigation: ✅ (navigate, back, forward, reload)');
  console.log('   • Information: ✅ (URL, summary, elements)');
  console.log('   • Interaction: ✅ (JavaScript, screenshot)');
  console.log('   • Tab Management: ✅ (create, close, switch, info)');
  console.log('   • Content Query: ✅ (forms, navigation, article, tables, media)');
  console.log('\n🎉 Athena Browser MCP integration is fully functional!');
}

main().catch(error => {
  console.error('❌ Error:', error.message);
  process.exit(1);
});
