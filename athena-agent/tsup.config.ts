import { defineConfig } from 'tsup';

export default defineConfig({
  // Entry points - main server files
  entry: ['src/server.ts', 'src/mcp-stdio-server.ts'],

  // Output pure ESM (Node.js native modules)
  format: ['esm'],

  // Generate TypeScript declaration files
  dts: true,

  // Clean output directory before build
  clean: true,

  // Generate sourcemaps for debugging
  sourcemap: true,

  // Output directory
  outDir: 'dist',

  // Preserve original file structure
  splitting: false,

  // Don't bundle dependencies (keep as external imports)
  // This is important for pure ESM - we want Node.js to resolve node_modules
  noExternal: [],

  // Target Node.js 18+ (matches package.json engines)
  target: 'node18',

  // Use .js extension for imports (Node.js ESM requirement)
  outExtension: () => ({ js: '.js' }),
});
