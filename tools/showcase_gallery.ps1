<#
.SYNOPSIS
  Captures matched baseline/enhanced showcase pairs from a local WRSR map package with the real
  OpenRepublic app (no assets are copied; outputs go wherever -OutputDir points, keep them out of Git).

.EXAMPLE
  .\tools\showcase_gallery.ps1 -Exe build\openrepublic-vs2022\src\app\Release\OpenRepublic.exe `
     -ContentRoot "C:\Games\SovietRepublic\media_soviet" -MapPackage "C:\Games\SovietRepublic\media_soviet\save_cloud\My save" `
     -OutputDir D:\captures -Building 61
#>
param(
    [Parameter(Mandatory)] [string]$Exe,
    [Parameter(Mandatory)] [string]$ContentRoot,
    [Parameter(Mandatory)] [string]$MapPackage,
    [Parameter(Mandatory)] [string]$OutputDir,
    [int]$Building = -1,
    [int]$Radius = 250
)
$views = @(
    @{ Name = 'golden-wide';  Camera = '-150 10 1.7'; Sun = '16 55' },
    @{ Name = 'golden-close'; Camera = '-110 17 1.0'; Sun = '17 45' },
    @{ Name = 'noon';         Camera = '-110 20 1.3'; Sun = '55 35' },
    @{ Name = 'dusk';         Camera = '-110 14 1.3'; Sun = '8 35' }
)
New-Item -ItemType Directory -Force $OutputDir | Out-Null
foreach ($v in $views) {
    foreach ($look in 'baseline', 'enhanced') {
        $out = Join-Path $OutputDir "$($v.Name)-$look.png"
        if (Test-Path $out) { continue }
        $arguments = @('--content-root', "`"$ContentRoot`"", '--map-package', "`"$MapPackage`"", '--map-region', '--map-radius', $Radius, '--map-lod', 0,
            '--showcase-capture', "`"$out`"", '--showcase-look', $look, '--showcase-camera') + $v.Camera.Split(' ')
        if ($Building -ge 0) { $arguments += @('--map-building', $Building) }
        if ($look -eq 'enhanced') { $arguments += @('--showcase-sun') + $v.Sun.Split(' ') }
        $p = Start-Process -FilePath $Exe -ArgumentList ($arguments -join ' ') -PassThru -Wait
        "$($v.Name) $look -> exit $($p.ExitCode)"
    }
}
