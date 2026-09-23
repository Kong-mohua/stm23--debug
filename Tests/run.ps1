$ErrorActionPreference = 'Stop'
Push-Location (Split-Path $PSScriptRoot -Parent)
try {
    $main = [IO.File]::ReadAllText((Join-Path $PWD 'Core\Src\main.c'))
    $keys = [regex]::Match($main, '(?s)#define KEY_NUM.*?(?=/\* USER CODE END 0 \*/)').Value
    if (!$keys) { throw 'Cannot locate the key scanner in main.c' }
    [IO.File]::WriteAllText((Join-Path $PSScriptRoot 'key_scan.inc'), $keys)
    & gcc -std=c99 -Wall -Wextra -Werror -ITests -ICore/Inc Tests/test_app.c Core/Src/oledfont.c Core/Src/cnfont.c -o Tests/test_app.exe
    if ($LASTEXITCODE -ne 0) { throw 'Host compilation failed' }
    & '.\Tests\test_app.exe'
    if ($LASTEXITCODE -ne 0) { throw 'Simulation failed' }
} finally { Pop-Location }
