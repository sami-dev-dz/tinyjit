# TinyJIT Studio - Lanceur PowerShell 1-clic
# Verifie les prerequis, compile le moteur C si besoin, et lance l'interface web

param(
    [string]$MsysPath = "C:\msys64\ucrt64\bin",
    [int]$Port = 8765
)

$env:PATH = "$MsysPath;" + $env:PATH

Write-Host "==========================================================" -ForegroundColor Cyan
Write-Host "       TINYJIT STUDIO — LANCEMENT DE L'ENVIRONNEMENT       " -ForegroundColor White
Write-Host "==========================================================" -ForegroundColor Cyan

# 1. Verifier my_jit_vm.exe
if (-not (Test-Path ".\my_jit_vm.exe")) {
    Write-Host "[Build] my_jit_vm.exe absent. Compilation automatique..." -ForegroundColor Yellow
    .\build.ps1
    if ($LASTEXITCODE -ne 0) {
        Write-Host "[Erreur] Echec de la compilation de my_jit_vm.exe." -ForegroundColor Red
        exit 1
    }
} else {
    Write-Host "[OK] Moteur C11 TinyJIT detecte (my_jit_vm.exe)" -ForegroundColor Green
}

# 2. Verifier Python
try {
    $pyVer = python --version 2>&1
    Write-Host "[OK] $pyVer detecte" -ForegroundColor Green
} catch {
    Write-Host "[Erreur] Python 3 est introuvable dans le PATH." -ForegroundColor Red
    exit 1
}

# 3. Lancer studio.py
Write-Host "[Studio] Demarrage du serveur FastAPI et de l'interface..." -ForegroundColor Cyan
python studio.py
