# setup-dev-junction.ps1
#
# Creates the Windows junction that lets the sample/ UE project see the
# plugin source at the repo's top-level plugin/ directory.
#
# Why: we keep the plugin at top level (it's the product), but UE requires
# plugins to live inside a project's Plugins/ folder. Rather than copy plugin
# files into sample/Plugins/UnrealMCP/ (two copies on disk, drift risk), we
# create a junction so there's one source of truth.
#
# Run from the repo root:
#   .\scripts\setup-dev-junction.ps1
#
# Idempotent: if the junction already exists pointing at the right target,
# it's left alone. If something else is at sample/Plugins/UnrealMCP/, the
# script refuses to clobber it.

$ErrorActionPreference = "Stop"

# Resolve repo root (parent of scripts/)
$repoRoot = Split-Path -Parent $PSScriptRoot
$pluginSource = Join-Path $repoRoot "plugin"
$junctionPath = Join-Path $repoRoot "sample\Plugins\UnrealMCP"
$pluginsDir   = Join-Path $repoRoot "sample\Plugins"

if (-not (Test-Path $pluginSource)) {
    Write-Error "Plugin source not found at: $pluginSource. Are you in the repo root?"
    exit 1
}

# Ensure sample/Plugins/ exists
if (-not (Test-Path $pluginsDir)) {
    New-Item -ItemType Directory -Path $pluginsDir -Force | Out-Null
    Write-Host "Created sample/Plugins/"
}

# Check if junction already exists and points at the right place
if (Test-Path $junctionPath) {
    $existing = Get-Item $junctionPath -Force
    if ($existing.Attributes -band [System.IO.FileAttributes]::ReparsePoint) {
        # It's a junction/symlink. Resolve its target.
        $existingTarget = $existing.Target
        if ($existingTarget -eq $pluginSource) {
            Write-Host "Junction already exists and points at the correct target."
            Write-Host "  $junctionPath -> $pluginSource"
            exit 0
        }
        Write-Warning "Junction exists but points elsewhere: $existingTarget"
        Write-Host "Removing the existing junction and recreating it."
        Remove-Item -Path $junctionPath -Force -Recurse
    } else {
        Write-Error "sample/Plugins/UnrealMCP exists but isn't a junction. Refusing to clobber. Move it aside and re-run."
        exit 1
    }
}

# Create the junction
cmd /c mklink /J "$junctionPath" "$pluginSource" | Out-Null

if ($LASTEXITCODE -ne 0) {
    Write-Error "Failed to create junction. mklink returned $LASTEXITCODE."
    exit 1
}

Write-Host "Created junction:"
Write-Host "  $junctionPath"
Write-Host "    -> $pluginSource"
Write-Host ""
Write-Host "You can now open sample/UnrealMCPSample.uproject in UE 5.7."
Write-Host "Edits to plugin/Source/ are immediately visible there."
