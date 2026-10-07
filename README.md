# demucs-ui-app

Standalone raylib GUI for Demucs stem separation.

- **macOS Apple Silicon:** [demucs-mac-silicon](https://github.com/nicapotato/demucs-mac-silicon) (MLX + Metal) via git submodule `workers/demucs-mac-silicon`
- **Windows x86_64:** [demucs-windows](https://github.com/nicapotato/demucs-windows) (official Demucs + CUDA) via git submodule `workers/demucs-windows`

Version: [`project.conf`](project.conf). Product names:

| Platform | App | Zip | itch channel |
|----------|-----|-----|--------------|
| Mac | `DemucsMlxArmMacSilicon.app` | `demucs-mlx-arm-mac-silicon.zip` | `mlx-arm-mac-silicon` |
| Windows | `DemucsCudaWindows.exe` | `demucs-cuda-windows.zip` | `cuda-windows` |

itch games: [demucs-app](https://nicapotato.itch.io/demucs-app) (ci) and `demucs-app-dev` (release).

## Clone

```bash
git clone --recurse-submodules https://github.com/nicapotato/demucs-ui-app.git
# existing checkout:
git submodule update --init --recursive
```

`fetch-depth` must be full if you pin submodule SHAs in CI (`actions/checkout` with `submodules: recursive` and `fetch-depth: 0`).

## Dev (macOS)

```bash
git submodule update --init --recursive
# MLX worker venv
(cd workers/demucs-mac-silicon && uv venv .venv --python 3.12 && source .venv/bin/activate && uv pip install -e '.[convert]' lameenc pyinstaller)

make -C app externals
make -C app models
make -C app run
make -C app version   # prints Demucs MLX ARM Mac Silicon x.y.z
make bundle           # .app + zip
```

## Windows zip (local, on a Windows box)

```powershell
git submodule update --init --recursive
cmake -B app/build -S app -DCMAKE_TOOLCHAIN_FILE=$env:VCPKG_ROOT/scripts/buildsystems/vcpkg.cmake -DVCPKG_TARGET_TRIPLET=x64-windows-static -DCMAKE_MSVC_RUNTIME_LIBRARY=MultiThreaded -A x64
cmake --build app/build --config Release --parallel
python -m pip install torch==2.2.2 torchaudio==2.2.2 --index-url https://download.pytorch.org/whl/cu121
python -m pip install -r workers/demucs-windows/requirements-windows.txt
$env:PYTHONPATH = (Resolve-Path workers/demucs-windows).Path
$env:DEMUCS_TORCH_PYTHON = (Get-Command python).Source
pwsh -File workers/demucs-windows/scripts/prefetch_torch_models.ps1
pwsh -File workers/demucs-windows/scripts/freeze_worker_torch.ps1
New-Item -ItemType Directory -Force -Path app/dist/worker | Out-Null
Copy-Item -Recurse -Force workers/demucs-windows/dist/worker/demucs_torch_worker app/dist/worker/
pwsh -File app/scripts/build_windows.ps1
# → app/dist/demucs-cuda-windows.zip
```

## Distribution

1. **itch.io** — https://nicapotato.itch.io/demucs-app (`mlx-arm-mac-silicon`, `cuda-windows`)
2. **GitHub Release** — zip + sha256 on a `v*` tag. Windows zip omitted if ≥ 2 GB.

Secret: `BUTLER_API_KEY` on this repo.

```bash
make ci                   # or: make ci-watch
make ci PUBLISH_ITCH=false
make ci BUILD_PLATFORM=windows
make release
```

## License

MIT. See `LICENSE`.
