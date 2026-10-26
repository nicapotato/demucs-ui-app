# Assemble a dedicated Windows zip for one worker flavor (torch, cuda, or onnx).
# The GUI is /MT + vcpkg x64-windows-static (no VC++ redist prompt).
$ErrorActionPreference = "Stop"

$Root = Split-Path -Parent $PSScriptRoot
$Repo = Split-Path -Parent $Root
$Variant = if ($env:DMX_VARIANT) { $env:DMX_VARIANT } else { "onnx" }

switch ($Variant) {
    "cuda" {
        $AppExec = "DemucsCudaWindows"
        $ZipName = "demucs-cuda-windows.zip"
        $Display = "Demucs CUDA Windows"
    }
    "onnx" {
        $AppExec = "DemucsOnnxWindows"
        $ZipName = "demucs-onnx-windows.zip"
        $Display = "Demucs ONNX Windows"
    }
    default {
        $Variant = "torch"
        $AppExec = "DemucsTorchWindows"
        $ZipName = "demucs-torch-windows.zip"
        $Display = "Demucs Torch Windows"
    }
}

$ExeCandidates = @(
    (Join-Path $Root "build\Release\$AppExec.exe"),
    (Join-Path $Root "build\$AppExec.exe")
)
$Exe = $ExeCandidates | Where-Object { Test-Path $_ } | Select-Object -First 1
$WorkerDir = Join-Path $Root "dist\worker\demucs_torch_worker"
$OnnxWorkerDir = Join-Path $Root "dist\worker\demucs_onnx_worker"
$CudaWorker = Join-Path $Root "dist\worker\demucs_cuda_worker.exe"
$Models = Join-Path $Root "models"
$Branding = Join-Path $Repo "resources"
$Release = Join-Path $Root "dist\$AppExec"
$ZipPath = Join-Path $Root "dist\$ZipName"

$Version = "0.0.0"
$Conf = Join-Path $Repo "project.conf"
if (Test-Path $Conf) {
    $line = Select-String -Path $Conf -Pattern "^VERSION=" | Select-Object -First 1
    if ($line) {
        $Version = ($line.Line -split "=", 2)[1].Trim()
        if (-not $Version) { $Version = "0.0.0" }
    }
}
Write-Host "Packaging $Display v$Version (variant: $Variant)"

if (-not $Exe) {
    Write-Error "Build the GUI first (CMake Release -DDMX_VARIANT=$Variant). Missing $AppExec.exe"
}

if ($Variant -eq "cuda") {
    if (-not (Test-Path $CudaWorker)) {
        Write-Error "Fetch or build the CUDA worker first: make -C app worker-fetch-cuda"
    }
} elseif ($Variant -eq "onnx") {
    if (-not (Test-Path (Join-Path $OnnxWorkerDir "demucs_onnx_worker.exe"))) {
        Write-Error "Fetch the ONNX worker first: make -C app worker-fetch-onnx ONNX_TARGET=windows-x64"
    }
    if (-not (Test-Path (Join-Path $Models "htdemucs_6s_fp16weights.onnx"))) {
        Write-Error "Missing models\htdemucs_6s_fp16weights.onnx. Run: make -C app worker-fetch-onnx ONNX_TARGET=windows-x64"
    }
} elseif (-not (Test-Path (Join-Path $WorkerDir "demucs_torch_worker.exe"))) {
    Write-Error "Build the torch worker first: make -C app worker-torch"
}

if (-not (Test-Path $Branding)) {
    Write-Error "Missing branding resources at $Branding"
}

if (Test-Path $Release) { Remove-Item -Recurse -Force $Release }
New-Item -ItemType Directory -Force -Path $Release | Out-Null
Copy-Item -Force $Exe (Join-Path $Release "$AppExec.exe")
if ($Variant -eq "cuda") {
    Write-Host "Worker: native CUDA ($CudaWorker)"
    New-Item -ItemType Directory -Force -Path (Join-Path $Release "worker") | Out-Null
    Copy-Item -Force $CudaWorker (Join-Path $Release "worker\demucs_cuda_worker.exe")
} elseif ($Variant -eq "onnx") {
    Write-Host "Worker: PyInstaller ONNX ($OnnxWorkerDir)"
    Copy-Item -Recurse -Force $OnnxWorkerDir (Join-Path $Release "worker")
} else {
    Write-Host "Worker: PyInstaller torch ($WorkerDir)"
    Copy-Item -Recurse -Force $WorkerDir (Join-Path $Release "worker")
}
New-Item -ItemType Directory -Force -Path (Join-Path $Release "models") | Out-Null
if (Test-Path $Models) {
    Copy-Item -Recurse -Force "$Models\*" (Join-Path $Release "models")
}
Copy-Item -Recurse -Force $Branding (Join-Path $Release "resources")

# GUI is statically linked (/MT + x64-windows-static). Do not copy msvcp140/vcruntime
# next to the exe — that reintroduces a VC++ redist dependency.

if (Test-Path $ZipPath) { Remove-Item -Force $ZipPath }
Compress-Archive -Path "$Release\*" -DestinationPath $ZipPath -Force
$bytes = (Get-Item $ZipPath).Length
Write-Host "Created $ZipPath ($bytes bytes)"
$under2g = $bytes -lt 2147483648
Write-Host "GH_RELEASE_OK=$under2g"
if ($env:GITHUB_OUTPUT) {
    "zip_bytes=$bytes" | Out-File -FilePath $env:GITHUB_OUTPUT -Append -Encoding utf8
    "gh_release_ok=$under2g" | Out-File -FilePath $env:GITHUB_OUTPUT -Append -Encoding utf8
}
