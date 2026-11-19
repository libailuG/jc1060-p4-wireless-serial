param(
    [ValidateSet('build','flash','monitor','menuconfig','size','fullclean')]
    [string]$Action = 'build',
    [string]$Port = 'COM13'
)
$ErrorActionPreference = 'Stop'
Push-Location $PSScriptRoot
try {
    $runner = Join-Path $PSScriptRoot '..\run-idf.ps1'
    $idfArgs = @($Action)
    if ($Action -in @('flash','monitor')) { $idfArgs = @('-p', $Port, $Action) }
    # 本机已安装 IDF6.1，使用统一环境脚本；其他机器在 IDF6.1 终端里运行。
    if (Test-Path -LiteralPath $runner) { & $runner @idfArgs }
    else { & idf.py @idfArgs }
    if ($LASTEXITCODE -ne 0) { throw "idf.py failed: $LASTEXITCODE" }
} finally {
    Pop-Location
}
