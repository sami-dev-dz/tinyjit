param(
    [string]$MsysPath = "C:\msys64\ucrt64\bin"
)

$env:PATH = "$MsysPath;" + $env:PATH

Write-Host "[Build] Compilation de TinyJIT..." -ForegroundColor Cyan
gcc -O2 -Wall -Wextra -std=c11 -Iinclude src/*.c -o my_jit_vm.exe -lgccjit

if ($LASTEXITCODE -eq 0) {
    Write-Host "[Build] Succes -> .\my_jit_vm.exe" -ForegroundColor Green
} else {
    Write-Host "[Build] Echec de la compilation." -ForegroundColor Red
    exit 1
}
