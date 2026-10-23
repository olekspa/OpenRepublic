<#
Applies the OpenRepublic Streamline patch set (tools/patches/wicked-streamline) in order to a Wicked Engine
checkout. Intended for a PRIVATE, opted-in build copy: patches the given checkout, never the pinned submodule
in place unless you pass it explicitly. No SDK or runtime binaries are fetched or committed.
Usage: pwsh tools/apply_streamline_patches.ps1 -WickedDir <path> [-Through 12] [-CheckOnly]
#>
param(
    [Parameter(Mandatory = $true)][string]$WickedDir,
    [ValidateRange(1, 99)][int]$Through = 8,
    [switch]$CheckOnly
)
$ErrorActionPreference = 'Stop'
$pinned = '4f93f3fd4f337aeb38dbfe410701b83024ccdd42'
$head = (git -C $WickedDir rev-parse HEAD).Trim()
if ($head -ne $pinned) { Write-Warning "Wicked HEAD $head differs from pinned $pinned; patches may not apply." }
$patches = Get-ChildItem (Join-Path $PSScriptRoot 'patches\wicked-streamline') -Filter '0*.patch' | Sort-Object Name |
    Where-Object { [int]($_.Name.Substring(0, 4)) -le $Through }
foreach ($p in $patches) {
    git -C $WickedDir apply --check $p.FullName
    if ($LASTEXITCODE -ne 0) { throw "Patch $($p.Name) does not apply cleanly" }
    if (-not $CheckOnly) { git -C $WickedDir apply $p.FullName; if ($LASTEXITCODE -ne 0) { throw "apply failed: $($p.Name)" } }
    Write-Host ("{0} {1}" -f ($(if ($CheckOnly) { 'ok   ' } else { 'applied' }), $p.Name))
}
