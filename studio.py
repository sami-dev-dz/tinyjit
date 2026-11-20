#!/usr/bin/env python3
"""
TinyJIT Studio Launcher
Lance le serveur FastAPI local et ouvre automatiquement l'interface Studio dans le navigateur par défaut.
"""

import sys
import time
import threading
import webbrowser
from pathlib import Path

def open_browser():
    """Attendre que le serveur démarre puis ouvrir le navigateur."""
    time.sleep(1.2)
    url = "http://127.0.0.1:8765"
    print(f"\n[TinyJIT Studio] Ouverture du navigateur vers {url} ...")
    webbrowser.open(url)

def main():
    root_dir = Path(__file__).resolve().parent
    exe_path = root_dir / "my_jit_vm.exe"
    
    if not exe_path.exists():
        print("[TinyJIT Studio] 'my_jit_vm.exe' non trouvé. Veuillez d'abord exécuter .\\build.ps1")
        sys.exit(1)

    print("=" * 60)
    print("           TINYJIT STUDIO — IDE & JIT VISUALIZER")
    print("=" * 60)
    print("  * Serveur : http://127.0.0.1:8765")
    print("  * Moteur  : my_jit_vm.exe (libgccjit x86-64)")
    print("  * Appuyez sur Ctrl+C dans ce terminal pour quitter.")
    print("=" * 60 + "\n")

    # Démarrage du thread pour ouvrir la page web
    threading.Thread(target=open_browser, daemon=True).start()

    # Lancement d'Uvicorn
    import uvicorn
    uvicorn.run("studio.server:app", host="127.0.0.1", port=8765, log_level="info")

if __name__ == "__main__":
    main()
