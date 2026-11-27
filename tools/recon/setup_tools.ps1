# The photo-measurement tools for the interior and exterior pipelines (docs/OSA_INTERIOR_PIPELINE.md section 4).
# Everything goes under one folder outside the repo (default E:\SAM3D-tools: C: is short of space), in its own venv,
# so nothing here touches the system Python the simcore tools use.
#
#   .\tools\recon\setup_tools.ps1 [-Root E:\SAM3D-tools] [-Skip torch,colmap,...]
#
# Installed (each with an open licence; none of the non-commercial or military-excluded models):
#   torch + torchvision (CUDA 12.8)    SAM 2.1, LightGlue, GeoCalib run on it
#   sam2 (Apache-2.0) + checkpoint      part outlines from point prompts
#   paddleocr 3 / PP-OCRv5 (Apache-2.0) Cyrillic plate OCR
#   lightglue (Apache-2.0)              ALIKED/DISK + LightGlue matching; SuperPoint weights are non-commercial: unused
#   hloc (Apache-2.0)                   the SfM front end over COLMAP
#   pycolmap (BSD)                      COLMAP from Python
#   geocalib (Apache-2.0, CC-BY weights) a focal-length prior, never a value
#   x-anylabeling (GPL, a separate app) clicking outlines with SAM; kept in its own venv (its pins clash)
#   COLMAP (BSD) Windows release        SfM / MVS binaries and GUI
#   Brush (Apache-2.0) Windows release  Gaussian-splat trainer and viewer (reference only)
#   tessdata_best rus + eng             Tesseract's best models, in Root\tessdata (set TESSDATA_PREFIX)
param(
    [string]$Root = 'E:\SAM3D-tools',
    [string[]]$Skip = @()
)
$ErrorActionPreference = 'Continue'  # 'Stop' would turn pip's and git's progress lines on stderr into errors (PS 5.1); exit codes are checked
$ProgressPreference = 'SilentlyContinue'
$py312 = 'C:\Users\Nick\AppData\Local\Programs\Python\Python312\python.exe'
if (-not (Test-Path $py312)) { $py312 = (Get-Command python).Source }
New-Item -ItemType Directory -Force "$Root\downloads", "$Root\pipcache", "$Root\tessdata", "$Root\models" | Out-Null
$env:PIP_CACHE_DIR = "$Root\pipcache"
$env:TMP = "$Root\downloads"; $env:TEMP = "$Root\downloads"
$py = "$Root\venv\Scripts\python.exe"
# E: may be a file system without ownership (exFAT): git refuses to work inside its clones, so pip can't install
# git+ URLs there. Clone, then install from the folder (no git needed).
function FromGit($url, $name, [string[]]$extra = @()) {
    $d = "$Root\src\$name"
    if (-not (Test-Path $d)) { git clone --depth 1 --recursive $url $d; if ($LASTEXITCODE) { throw "git clone $url failed" } }
    Pip (@('--no-build-isolation', $d) + $extra)
}
function Want($name) { -not ($Skip -contains $name) }
function Pip([string[]]$a) { & $py -m pip install @a; if ($LASTEXITCODE) { throw "pip install $a failed" } }

if (-not (Test-Path $py)) { & $py312 -m venv "$Root\venv"; & $py -m pip install --upgrade pip }

if (Want 'torch') { Pip @('torch', 'torchvision', '--index-url', 'https://download.pytorch.org/whl/cu128') }
if (Want 'base') { Pip @('numpy', 'scipy', 'opencv-contrib-python', 'pillow', 'matplotlib', 'tqdm', 'huggingface_hub') }
if (Want 'sam2') {
    $env:SAM2_BUILD_CUDA = '0'  # its optional CUDA post-processing extension needs nvcc; SAM 2 runs without it
    FromGit 'https://github.com/facebookresearch/sam2.git' 'sam2'
    $ck = "$Root\models\sam2.1_hiera_base_plus.pt"
    if (-not (Test-Path $ck)) { Invoke-WebRequest 'https://dl.fbaipublicfiles.com/segment_anything_2/092824/sam2.1_hiera_base_plus.pt' -OutFile $ck }
}
if (Want 'ocr') { Pip @('paddlepaddle', 'paddleocr') }
if (Want 'lightglue') { FromGit 'https://github.com/cvg/LightGlue.git' 'lightglue' }
if (Want 'colmap') {
    Pip @('pycolmap')
    if (-not (Test-Path "$Root\colmap")) {
        $rel = Invoke-RestMethod 'https://api.github.com/repos/colmap/colmap/releases/latest'
        $asset = $rel.assets | Where-Object { $_.name -match 'x64-windows-cuda\.zip$' } | Select-Object -First 1
        $zip = "$Root\downloads\$($asset.name)"
        Invoke-WebRequest $asset.browser_download_url -OutFile $zip
        Expand-Archive $zip "$Root\colmap"
    }
}
if (Want 'hloc') {
    # its requirements name LightGlue by git URL (the same ownership problem): LightGlue comes from its own folder
    # above, so hloc goes in without dependencies and its others are named here (opencv-contrib stands for opencv).
    # Its third_party SuperGlue/SuperPoint weights are non-commercial: unused (ALIKED or DISK + LightGlue).
    Pip @('tqdm', 'matplotlib', 'plotly', 'scipy', 'h5py', 'kornia', 'gdown', 'pycolmap')
    FromGit 'https://github.com/cvg/Hierarchical-Localization.git' 'hloc' @('--no-deps')
}
if (Want 'geocalib') { FromGit 'https://github.com/cvg/GeoCalib.git' 'geocalib' }
if (Want 'tessdata') {
    foreach ($l in 'rus', 'eng') {
        $f = "$Root\tessdata\$l.traineddata"
        if (-not (Test-Path $f)) { Invoke-WebRequest "https://github.com/tesseract-ocr/tessdata_best/raw/main/$l.traineddata" -OutFile $f }
    }
    Copy-Item 'C:\Program Files\Tesseract-OCR\tessdata\osd.traineddata' "$Root\tessdata\" -ErrorAction SilentlyContinue
    # the output formats (tsv, hocr, ...) are config files in tessdata: --tessdata-dir needs them beside the models
    Copy-Item 'C:\Program Files\Tesseract-OCR\tessdata\configs', 'C:\Program Files\Tesseract-OCR\tessdata\tessconfigs' "$Root\tessdata\" -Recurse -Force -ErrorAction SilentlyContinue
}
if (Want 'anylabeling') {
    if (-not (Test-Path "$Root\venv-label\Scripts\python.exe")) { & $py312 -m venv "$Root\venv-label" }
    & "$Root\venv-label\Scripts\python.exe" -m pip install --upgrade pip
    & "$Root\venv-label\Scripts\python.exe" -m pip install x-anylabeling-cvhub
}
if (Want 'depth') {
    # The holistic layer's depth comparison (docs/OSA_INTERIOR_HOLISTIC.md sections 31-33): MoGe-2 (depth and normals, MIT), trimesh with Embree for rays outside Blender, pycolmap and scipy.
    # Its own venv, because MoGe's requirements would move Pillow and OpenCV under the photo tools; it reuses venv's torch through a .pth. MoGe-3 does not run on Windows (Triton).
    $dp = "$Root\venv-depth\Scripts\python.exe"
    if (-not (Test-Path $dp)) { & $py312 -m venv "$Root\venv-depth"; & $dp -m pip install --upgrade pip }
    Set-Content -Encoding ascii "$Root\venv-depth\Lib\site-packages\zz_base.pth" "$Root\venv\Lib\site-packages"
    $d = "$Root\src\MoGe"
    if (-not (Test-Path $d)) { git clone --depth 1 https://github.com/microsoft/MoGe.git $d; if ($LASTEXITCODE) { throw 'git clone MoGe failed' } }
    & $dp -m pip install $d trimesh embreex pycolmap scipy
    if ($LASTEXITCODE) { throw 'installing the depth environment failed' }
    # The weights (Ruicheng/moge-2-vitl-normal) come on first use: set HF_HOME=$Root\models\hf
}
if (Want 'brush') {
    if (-not (Test-Path "$Root\brush")) {
        $rel = Invoke-RestMethod 'https://api.github.com/repos/ArthurBrussee/brush/releases/latest'
        $asset = $rel.assets | Where-Object { $_.name -match 'windows-msvc\.zip$' } | Select-Object -First 1
        $sum = $rel.assets | Where-Object { $_.name -eq "$($asset.name).sha256" } | Select-Object -First 1
        $f = "$Root\downloads\$($asset.name)"
        Invoke-WebRequest $asset.browser_download_url -OutFile $f
        if ($sum) {  # the release's own checksum
            $want = ((Invoke-WebRequest $sum.browser_download_url).Content -split '\s+')[0]
            if ((Get-FileHash $f -Algorithm SHA256).Hash -ne $want.ToUpper()) { throw "Brush download's checksum does not match" }
        }
        Expand-Archive $f "$Root\brush"
    }
}
Write-Host "Done. Python: $py   COLMAP: $Root\colmap   Brush: $Root\brush   TESSDATA_PREFIX=$Root\tessdata"


