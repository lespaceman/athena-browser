#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"

echo "🎨 Building Homepage..."

cd "$ROOT_DIR/homepage"

# Ensure dependencies are installed
if [ ! -d "node_modules" ]; then
  echo "📥 Installing homepage dependencies..."
  npm install
fi

npm run build

echo "📦 Copying homepage to resources/homepage/..."
rm -rf "$ROOT_DIR/resources/homepage/"*
cp -r dist/* "$ROOT_DIR/resources/homepage/"

echo "✅ Homepage build complete: resources/homepage/"
