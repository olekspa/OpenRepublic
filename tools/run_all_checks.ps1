[CmdletBinding()]
param(
    [string]$BuildDirectory = "build/headless",
    [string]$HostExecutable,
    [ValidateRange(1, 100000)][int]$ScenarioTicks = 600
)
$ErrorActionPreference = 'Stop'
$repo = Split-Path -Parent $PSScriptRoot
Push-Location $repo
try {
    Write-Host "OpenRepublic synthetic validation | $(Get-Date -Format 'yyyy-MM-dd HH:mm:ss zzz')"
    cmake -S . -B $BuildDirectory -DOPENREPUBLIC_BUILD_CLIENT=OFF
    if ($LASTEXITCODE -ne 0) { throw "CMake configure failed ($LASTEXITCODE)" }
    cmake --build $BuildDirectory --config Release -- /m
    if ($LASTEXITCODE -ne 0) { throw "Release build failed ($LASTEXITCODE)" }
    ctest --test-dir $BuildDirectory -C Release --output-on-failure
    if ($LASTEXITCODE -ne 0) { throw "CTest failed ($LASTEXITCODE)" }

    $economyHost = $HostExecutable
    if (-not $economyHost) {
        $economyHost = Get-ChildItem -LiteralPath $BuildDirectory -Filter OpenRepublicEconomyHost.exe -Recurse -File |
            Select-Object -First 1 -ExpandProperty FullName
    }
    if (-not $economyHost) { throw "OpenRepublicEconomyHost.exe not found below $BuildDirectory" }
    if (-not (Test-Path -LiteralPath $economyHost -PathType Leaf)) { throw "Host executable not found: $economyHost" }
    $scenarios = Get-ChildItem -LiteralPath 'data/scenarios' -Filter '*.json' -File | Sort-Object Name
    if (-not $scenarios) { throw 'No scenario files found' }
    $rows = [System.Collections.Generic.List[object]]::new()
    foreach ($scenario in $scenarios) {
        $hashes = @()
        $scoreUnits = ''
        for ($run = 0; $run -lt 2; $run++) {
            $raw = & python tools/scenario_score.py --host $economyHost --scenario $scenario.FullName --ticks $ScenarioTicks
            if ($LASTEXITCODE -ne 0) { throw "Scorer failed: $($scenario.Name) run $($run + 1)" }
            $receipt = ($raw -join "`n") | ConvertFrom-Json
            if (-not $receipt.scoreHash) { throw "Missing scoreHash: $($scenario.Name)" }
            $hashes += [string]$receipt.scoreHash
            $scoreUnits = [string]$receipt.scoreUnits
        }
        if ($hashes[0] -ne $hashes[1]) { throw "Repeat hash mismatch: $($scenario.Name)" }
        $rows.Add([pscustomobject]@{ Scenario = $scenario.BaseName; Score = $scoreUnits; Hash = $hashes[0].Substring(0, 12) })
    }
    Write-Host "`nPASS | CTest passed | scenarios $($rows.Count)/$($rows.Count) | repeat hashes matched | ticks=$ScenarioTicks"
    $rows | Format-Table -AutoSize | Out-Host
} finally {
    Pop-Location
}
