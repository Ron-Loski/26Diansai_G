param(
    [ValidateSet('formal','checker','signaltap')]
    [string]$Mode = 'formal'
)

$qsf = Join-Path $PSScriptRoot '..\prj\DAC904_HighSpeedDAC.qsf'
$checkerTemplate = Join-Path $PSScriptRoot '..\debug\DAC904_HighSpeedDAC_checker.qsf'
if (($Mode -ne 'formal') -and (Test-Path -LiteralPath $checkerTemplate)) {
    $text = Get-Content -LiteralPath $checkerTemplate -Raw
} else {
    $text = Get-Content -LiteralPath $qsf -Raw
}
$enableTap = ($Mode -ne 'formal')
$text = $text -replace 'set_global_assignment -name ENABLE_SIGNALTAP (ON|OFF)',
    ('set_global_assignment -name ENABLE_SIGNALTAP ' + ($(if ($enableTap) {'ON'} else {'OFF'})))
$text = $text -replace 'set_global_assignment -name ENABLE_LOGIC_ANALYZER_INTERFACE (ON|OFF)',
    ('set_global_assignment -name ENABLE_LOGIC_ANALYZER_INTERFACE ' + ($(if ($enableTap) {'ON'} else {'OFF'})))
$text = $text -replace '(?m)^set_parameter -name ADC_TEST_MODE .*$\r?\n?', ''
if ($Mode -eq 'checker') {
    $text += "`r`nset_parameter -name ADC_TEST_MODE 4`r`n"
}
if ($Mode -eq 'formal') {
    $kept = $text -split "`r?`n" | Where-Object {
        $_ -notmatch 'SIGNALTAP|SLD_|sld_|CONNECT_TO_SLD|LOGIC_ANALYZER'
    }
    $text = ($kept -join "`r`n") + "`r`n"
}
Set-Content -LiteralPath $qsf -Value $text -Encoding Ascii
Write-Host "Configured $Mode build in $qsf"
