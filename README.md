# demucs-ui-app

Standalone raylib GUI for Demucs stem separation. One download per worker;
install the app that matches the backend you want.

- **macOS Apple Silicon:** [demucs-mac-silicon](https://github.com/nicapotato/demucs-mac-silicon) (MLX + Metal). CI downloads the worker pinned by `MAC_WORKER_TAG` in [`project.conf`](project.conf).
- **Windows x86_64 torch:** [demucs-windows](https://github.com/nicapotato/demucs-windows) (official Demucs + CUDA torch).
- **Windows x86_64 cuda:** [demucs-cuda](https://github.com/nicapotato/demucs-cuda) (native C + CUDA). CI downloads the worker pinned by `CUDA_WORKER_TAG` in [`project.conf`](project.conf).

Version: [`project.conf`](project.conf). Product names:

| Flavor | App | Zip | itch channel |
|--------|-----|-----|--------------|
| mlx-c (mac arm) | `DemucsMlxCArmMacSilicon.app` | `demucs-mlx-c-arm-mac-silicon.zip` | `mlx-c-arm-mac-silicon` |
| mlx-py (mac arm) | `DemucsMlxPyArmMacSilicon.app` | `demucs-mlx-py-arm-mac-silicon.zip` | `mlx-py-arm-mac-silicon` |
| torch (Windows NVIDIA) | `DemucsTorchWindows.exe` | `demucs-torch-windows.zip` | `torch-windows` |
| cuda (Windows NVIDIA) | `DemucsCudaWindows.exe` | `demucs-cuda-windows.zip` | `cuda-windows` |

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
brew install cmake libvorbis libogg ffmpeg
# gh must be logged in to download the pinned worker release

make -C app externals
make -C app worker-fetch-c   # C worker + weights from MAC_WORKER_TAG
make -C app run              # DemucsMlxCArmMacSilicon
make -C app version
make bundle                  # mlx-c .app + zip
make bundle-mlx-py           # mlx-py .app + zip
```

CMake flavor: `-DDMX_VARIANT=mlx-c` (default on mac) or `mlx-py`.

From-source worker builds (compiles mlx-c, slow) still work via `make -C app worker` /
`make -C app models`. A sibling `../demucs-mac-silicon` checkout is used when present;
override with `C_MLX=...`. Dev Python: `make -C app run-py`.

## Windows zip (local, on a Windows box)

```powershell
git submodule update --init --recursive
cmake -B app/build -S app -DCMAKE_TOOLCHAIN_FILE=$env:VCPKG_ROOT/scripts/buildsystems/vcpkg.cmake -DVCPKG_TARGET_TRIPLET=x64-windows-static -DCMAKE_MSVC_RUNTIME_LIBRARY=MultiThreaded -DDMX_VARIANT=torch -A x64
cmake --build app/build --config Release --parallel
python -m pip install torch==2.2.2 torchaudio==2.2.2 --index-url https://download.pytorch.org/whl/cu121
python -m pip install -r workers/demucs-windows/requirements-windows.txt
$env:PYTHONPATH = (Resolve-Path workers/demucs-windows).Path
$env:DEMUCS_TORCH_PYTHON = (Get-Command python).Source
pwsh -File workers/demucs-windows/scripts/prefetch_torch_models.ps1
pwsh -File workers/demucs-windows/scripts/freeze_worker_torch.ps1
New-Item -ItemType Directory -Force -Path app/dist/worker | Out-Null
Copy-Item -Recurse -Force workers/demucs-windows/dist/worker/demucs_torch_worker app/dist/worker/
$env:DMX_VARIANT = "torch"
pwsh -File app/scripts/build_windows.ps1
# → app/dist/demucs-torch-windows.zip
```

## Windows CUDA zip (local, on a Windows box)

Hosted CI has no NVIDIA GPU, so it only smokes `--version` and `demucs_cuda_worker --list-models`. Real separation needs a Turing+ GPU and driver R570+.

```powershell
git submodule update --init --recursive
cmake -B app/build -S app -DCMAKE_TOOLCHAIN_FILE=$env:VCPKG_ROOT/scripts/buildsystems/vcpkg.cmake -DVCPKG_TARGET_TRIPLET=x64-windows-static -DCMAKE_MSVC_RUNTIME_LIBRARY=MultiThreaded -DDMX_VARIANT=cuda -A x64
cmake --build app/build --config Release --parallel
# gh must be logged in to download CUDA_WORKER_TAG
bash app/scripts/fetch_cuda_worker.sh
$env:DMX_VARIANT = "cuda"
pwsh -File app/scripts/build_windows.ps1
# → app/dist/demucs-cuda-windows.zip
```

## Distribution

1. **itch.io** — https://nicapotato.itch.io/demucs-app (`mlx-c-arm-mac-silicon`, `mlx-py-arm-mac-silicon`, `torch-windows`) and https://nicapotato.itch.io/demucs-app-dev (same channels plus `cuda-windows`)
2. **GitHub Release** — zip + sha256 on a `v*` tag. Windows zip omitted if ≥ 2 GB.

Secret: `BUTLER_API_KEY` on this repo.

```bash
make ci                   # or: make ci-watch
make ci PUBLISH_ITCH=false
make ci BUILD_PLATFORM=macos-c
make ci BUILD_PLATFORM=windows
make release BUILD_PLATFORM=windows-cuda PUBLISH_GH_RELEASE=false
make release
```

## License

MIT. See `LICENSE`.
