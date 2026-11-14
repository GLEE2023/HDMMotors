# Run from an extracted frontend folder to copy source into a local HDMMotors checkout.
# Does not commit or push; review the changes before using Git.
param([Parameter(Mandatory=$true)][string]$RepoPath)
$ErrorActionPreference = 'Stop'
$target = (Resolve-Path $RepoPath).Path
if (-not (Test-Path (Join-Path $target '.git'))) { throw 'RepoPath must point to an HDMMotors git checkout' }
$dest = Join-Path $target 'react_frontend'
if (Test-Path $dest) { throw 'react_frontend already exists; copy/merge changes manually to preserve existing work.' }
New-Item -ItemType Directory -Force -Path $dest | Out-Null
Get-ChildItem $PSScriptRoot -Force | Where-Object { $_.Name -ne 'install-into-repo.ps1' } | Copy-Item -Destination $dest -Recurse -Force
Write-Host "Copied React frontend to $dest"
Write-Host 'Review: git diff --stat && git status'
Write-Host 'Requested commit message: created new frontend using React, including assembly'
