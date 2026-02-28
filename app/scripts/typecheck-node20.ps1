$ErrorActionPreference = 'Stop'

$ProjectRoot = Split-Path -Parent $PSScriptRoot

# Run type-check inside isolated Node 20 container without changing local Node.
docker run --rm `
  -v "${ProjectRoot}:/workspace" `
  -w /workspace `
  node:20-bullseye `
  sh -lc "npm ci && npm run type-check"
