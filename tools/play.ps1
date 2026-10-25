param(
    [ValidateSet('coal', 'sites', 'town')]
    [string]$Preset = 'coal',
    [string]$Executable = (Join-Path $PSScriptRoot '..\build\openrepublic-vs2022\src\app\Release\OpenRepublic.exe'),
    [Parameter(ValueFromRemainingArguments = $true)]
    [string[]]$AdditionalArguments = @()
)
$resolved = (Resolve-Path -LiteralPath $Executable -ErrorAction Stop).Path
$workingDirectory = Split-Path -Parent $resolved
Push-Location $workingDirectory
try {
    & $resolved --play $Preset @AdditionalArguments
    exit $LASTEXITCODE
}
finally {
    Pop-Location
}
