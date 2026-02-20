$ErrorActionPreference = "Stop"

# Paths
$StageDir = "stage/docs/html"
$RepoRoot = (Get-Location).Path
$GhPagesDir = Join-Path $RepoRoot "gh-pages-temp"

# Clean temp clone
if (Test-Path $GhPagesDir) { Remove-Item -Recurse -Force $GhPagesDir }

# Clone gh-pages branch
git clone --branch gh-pages --single-branch (git config --get remote.origin.url) $GhPagesDir

# Remove old docs
Remove-Item -Recurse -Force (Join-Path $GhPagesDir "*")

# Copy new docs
Copy-Item -Recurse -Force -Path (Join-Path $StageDir "*") -Destination $GhPagesDir

# Commit & push
Push-Location $GhPagesDir
git add .
git commit -m "Update docs $(Get-Date -Format yyyy-MM-dd)"
git push origin gh-pages
Pop-Location

Write-Host "Documentation deployed to GitHub Pages."