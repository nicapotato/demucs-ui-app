Application that can take mp3, wav, or PSARC (Rocksmith 2014 CDLC) and extract mp3 or wav stems (isolated tracks for drums, guitar, bass, vocals, piano) using Demucs (6-stem `htdemucs_6s`).

Download the build that matches your machine.

# macOS Apple Silicon — MLX C (~124 MB)

`demucs-mlx-c-arm-mac-silicon.zip`

Native C worker on MLX + Metal. No PyTorch. Small download, unified memory.

# macOS Apple Silicon — MLX Python (~144 MB)

`demucs-mlx-py-arm-mac-silicon.zip`

Frozen [demucs-mlx](https://github.com/ssmall256/demucs-mlx/) worker (MLX + Metal). No PyTorch. Typically far faster than realtime (about 73× on an M4 Max for 4-stem `htdemucs`).

# Windows x64 — PyTorch (~2 GB)

`demucs-torch-windows.zip`

Official Meta Demucs + PyTorch. The zip is large because it includes the CUDA runtime — you only need a recent NVIDIA driver, not a CUDA toolkit. NVIDIA GPU recommended (about 5–7 GB VRAM for the default 6-stem model). CPU works but is slow (a 4-minute track can take about a minute). AMD and Intel GPUs fall back to CPU.

Default model is Demucs `htdemucs_6s`. Output is MP3 or WAV in a RocknRoller-compatible folder layout so you can mute the chart guitar and play over the rest of the band.

**Repos:**

- App: https://github.com/nicapotato/demucs-ui-app
- macOS workers: https://github.com/nicapotato/demucs-mac-silicon
- Windows PyTorch worker: https://github.com/nicapotato/demucs-windows
- Original MLX port (MLX Python build): https://github.com/ssmall256/demucs-mlx/

Pairs with [RocknRoller](https://nicapotato.itch.io/rocknroller).

NICAPOTATO Discord Community: https://discord.gg/7djjRNECz
