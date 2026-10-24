param(
    [Parameter(Mandatory = $true, Position = 0)]
    [string] $DefinitionsDirectory,
    [string] $BuildDirectory = "build/headless"
)

$ErrorActionPreference = "Stop"
$inputRoot = (Resolve-Path -LiteralPath $DefinitionsDirectory).Path
if (-not (Test-Path -LiteralPath $inputRoot -PathType Container)) {
    throw "Definitions directory does not exist: $DefinitionsDirectory"
}
$root = $inputRoot
$candidate = Join-Path $inputRoot "media_soviet/buildings_types"
if (Test-Path -LiteralPath $candidate -PathType Container) {
    $root = (Resolve-Path -LiteralPath $candidate).Path
} elseif ((Split-Path -Leaf $inputRoot) -ne "buildings_types") {
    $candidate = Join-Path $inputRoot "buildings_types"
    if (Test-Path -LiteralPath $candidate -PathType Container) {
        $root = (Resolve-Path -LiteralPath $candidate).Path
    }
}
$probe = Join-Path (Resolve-Path -LiteralPath $BuildDirectory).Path "OpenRepublicCatalogCoverageProbe.exe"
if (-not (Test-Path -LiteralPath $probe -PathType Leaf)) {
    throw "Probe not built. Build OpenRepublicCatalogCoverageProbe in $BuildDirectory first."
}
& $probe --root $root
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
