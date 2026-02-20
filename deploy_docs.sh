#!/bin/bash
set -e

# Paths
STAGE_DIR=stage/docs/html
REPO_ROOT=$(pwd)
GH_PAGES_DIR=$REPO_ROOT/gh-pages-temp

# Clean temp clone
rm -rf "$GH_PAGES_DIR"

# Clone gh-pages branch into temp folder
git clone --branch gh-pages --single-branch "$(git config --get remote.origin.url)" "$GH_PAGES_DIR"

# Remove old docs
rm -rf "$GH_PAGES_DIR"/*

# Copy new docs
cp -r "$STAGE_DIR"/* "$GH_PAGES_DIR"/

# Commit & push
cd "$GH_PAGES_DIR"
git add .
git commit -m "Update docs $(date +%Y-%m-%d)"
git push origin gh-pages

echo "Documentation deployed to GitHub Pages."