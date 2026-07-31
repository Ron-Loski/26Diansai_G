$ErrorActionPreference = 'Stop'
$modelsim = 'E:\Modelsim\win64'
python "$PSScriptRoot\generate_multistage_vectors.py"
Push-Location $PSScriptRoot
try {
    if (Test-Path work) { Remove-Item -Recurse -Force work }
    & "$modelsim\vlib.exe" work
    & "$modelsim\vlog.exe" -work work `
        ..\rtl\FirDecimatingStage.V `
        ..\rtl\Ad9233MultistageDecimator.V `
        ..\rtl\Ad9233SpiInit.V `
        ..\rtl\AnalyzerSpiSlave.V `
        tb_ad9233_multistage_decimator.v `
        tb_ad9233_spi_init.v `
        tb_analyzer_spi_slave.v
    foreach ($test in @('tb_ad9233_multistage_decimator',
                         'tb_ad9233_spi_init', 'tb_analyzer_spi_slave')) {
        $logDir = Join-Path $PSScriptRoot '..\reports\62m5\modelsim'
        New-Item -ItemType Directory -Path $logDir -Force | Out-Null
        & "$modelsim\vsim.exe" -c -l (Join-Path $logDir "$test.log") `
            -do "run -all; quit -f" "work.$test"
        if ($LASTEXITCODE -ne 0) { throw "$test failed" }
    }
} finally {
    Pop-Location
}
