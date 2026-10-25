param(
    [Parameter(Mandatory = $true)][string]$ReleaseFolder,
    [Parameter(Mandatory = $true)][string]$BuiltExe,
    [string]$StageDirectory = 'C:\or-wt-builds\planner-run',
    [int]$TimeoutSeconds = 240
)

$ErrorActionPreference = 'Stop'
$repoRoot = Split-Path -Parent $PSScriptRoot
$sourceScripts = Join-Path $repoRoot 'data\ui_scripts'
$sourceCampaigns = Join-Path $repoRoot 'data\campaign'
if (!(Test-Path -LiteralPath $ReleaseFolder -PathType Container)) { throw "Release folder not found: $ReleaseFolder" }
if (!(Test-Path -LiteralPath $BuiltExe -PathType Leaf)) { throw "Built OpenRepublic.exe not found: $BuiltExe" }
if (!(Test-Path -LiteralPath $sourceScripts -PathType Container)) { throw "UI scripts not found: $sourceScripts" }

New-Item -ItemType Directory -Path $StageDirectory -Force | Out-Null
Copy-Item -Path (Join-Path $ReleaseFolder '*') -Destination $StageDirectory -Recurse -Force
Copy-Item -Path $BuiltExe -Destination (Join-Path $StageDirectory 'OpenRepublic.exe') -Force
$stagedData = Join-Path $StageDirectory 'data'
if (Test-Path -LiteralPath $stagedData) { Remove-Item -LiteralPath $stagedData -Recurse -Force }
New-Item -ItemType Directory -Path $stagedData -Force | Out-Null
Get-ChildItem -LiteralPath (Join-Path $repoRoot 'data') | Copy-Item -Destination $stagedData -Recurse -Force
$exe = Join-Path $StageDirectory 'OpenRepublic.exe'
$runs = @(
    @{ Name = 'UI actions'; Script = 'actions_smoke.json'; Args = @('--native-economy', '--native-town-demo', '--native-objectives', 'builtin:growth_town') },
    @{ Name = 'Preset coal'; Script = 'coal_smoke.json'; Args = @('--play', 'coal', '--autoplay', '--autoplay-speed', '10') },
    @{ Name = 'Preset sites'; Script = 'sites_smoke.json'; Args = @('--play', 'sites', '--autoplay', '--autoplay-speed', '10') },
    @{ Name = 'Preset town'; Script = 'town_smoke.json'; Args = @('--play', 'town', '--autoplay', '--autoplay-speed', '10') },
    @{ Name = 'Campaign coal_to_heat'; Script = 'campaign_coal_to_heat_smoke.json'; Mission = 'coal_to_heat' },
    @{ Name = 'Campaign build_the_chain'; Script = 'campaign_build_the_chain_smoke.json'; Mission = 'build_the_chain' },
    @{ Name = 'Campaign growing_town'; Script = 'campaign_growing_town_smoke.json'; Mission = 'growing_town' },
    @{ Name = 'Campaign power_and_water'; Script = 'campaign_power_and_water_smoke.json'; Mission = 'power_and_water' },
    @{ Name = 'Campaign trade_and_logistics'; Script = 'campaign_trade_and_logistics_smoke.json'; Mission = 'trade_and_logistics' }
)
$missionOrder = @('coal_to_heat', 'build_the_chain', 'growing_town', 'power_and_water', 'trade_and_logistics')
$oldLocalAppData = $env:LOCALAPPDATA
$results = [System.Collections.Generic.List[object]]::new()

function Quote-Argument([string]$Value) {
    if ($Value.Contains(' ') -or $Value.Contains('"')) { return '"' + $Value.Replace('"', '\"') + '"' }
    return $Value
}

try {
    foreach ($run in $runs) {
        $scriptPath = Join-Path $StageDirectory (Join-Path 'data\ui_scripts' $run.Script)
        if (!(Test-Path -LiteralPath $scriptPath -PathType Leaf)) { throw "Smoke script missing from stage: $scriptPath" }
        $spec = Get-Content -Raw -LiteralPath $scriptPath | ConvertFrom-Json
        $screenshots = @($spec.actions | Where-Object { $_.action -eq 'screenshot' } | ForEach-Object { Join-Path $StageDirectory $_.path })
        $arguments = @()
        if ($run.Mission) {
            $runLocal = Join-Path $StageDirectory (Join-Path 'smoke-localappdata' $run.Mission)
            $saveDirectory = Join-Path $runLocal 'OpenRepublic\saves'
            New-Item -ItemType Directory -Path $saveDirectory -Force | Out-Null
            $progress = "OPENREPUBLIC_CAMPAIGN_PROGRESS 1`ncampaign=republic_rising`n"
            foreach ($mission in $missionOrder) {
                $done = [Array]::IndexOf($missionOrder, $mission) -lt [Array]::IndexOf($missionOrder, $run.Mission)
                $progress += "$mission|$(if ($done) { '1' } else { '0' })|0`n"
            }
            [IO.File]::WriteAllText((Join-Path $saveDirectory 'campaign-republic_rising-progress.dat'), $progress, [Text.UTF8Encoding]::new($false))
            $env:LOCALAPPDATA = $runLocal
            $arguments += @('--campaign', 'republic_rising', '--autoplay', '--autoplay-speed', '10')
        } else {
            $runLocal = Join-Path $StageDirectory (Join-Path 'smoke-localappdata' ($run.Script -replace '\.json$', ''))
            New-Item -ItemType Directory -Path $runLocal -Force | Out-Null
            $env:LOCALAPPDATA = $runLocal
            $arguments += $run.Args
        }
        $arguments += @('--ui-script', (Join-Path 'data\ui_scripts' $run.Script))
        $quoted = @($arguments | ForEach-Object { Quote-Argument ([string]$_) }) -join ' '
        $process = Start-Process -FilePath $exe -ArgumentList $quoted -WorkingDirectory $StageDirectory -PassThru
        $exited = $process.WaitForExit($TimeoutSeconds * 1000)
        if (!$exited) {
            Stop-Process -Id $process.Id -Force -ErrorAction SilentlyContinue
            $exitCode = 'TIMEOUT'
        } else {
            $exitCode = $process.ExitCode
        }
        $missingCaptures = @($screenshots | Where-Object { !(Test-Path -LiteralPath $_ -PathType Leaf) })
        $passed = $exitCode -eq 0 -and $missingCaptures.Count -eq 0
        $results.Add([pscustomobject]@{ Run = $run.Name; Result = $(if ($passed) { 'PASS' } else { 'FAIL' }); ExitCode = $exitCode; Screenshots = ($screenshots | ForEach-Object { Split-Path -Leaf $_ }) -join ', '; Missing = $missingCaptures.Count })
        if (!$passed) { Write-Warning "$($run.Name) failed (exit $exitCode); missing screenshots: $($missingCaptures -join ', ')" }
        if ($oldLocalAppData) { $env:LOCALAPPDATA = $oldLocalAppData } else { Remove-Item Env:LOCALAPPDATA -ErrorAction SilentlyContinue }
    }
} finally {
    if ($oldLocalAppData) { $env:LOCALAPPDATA = $oldLocalAppData } else { Remove-Item Env:LOCALAPPDATA -ErrorAction SilentlyContinue }
}

$results | Format-Table -AutoSize
$failed = @($results | Where-Object Result -ne 'PASS')
Write-Host "Staged executable: $exe"
Write-Host "Screenshots: $StageDirectory"
Write-Host "Passed $($results.Count - $failed.Count)/$($results.Count) runs."
if ($failed.Count) { exit 1 }
