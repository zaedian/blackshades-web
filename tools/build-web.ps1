param(
    [switch]$Clean,
    [switch]$Release
)

$ErrorActionPreference = "Stop"

$Root = Split-Path -Parent $PSScriptRoot
$SourceDir = Join-Path $Root "source\blackshades\Source"
$WebDir = Join-Path $Root "source\blackshades\web"
$EmscriptenDir = Join-Path $Root "emsdk\upstream\emscripten"
$Emxx = Join-Path $EmscriptenDir "em++.exe"
$DocsDir = Join-Path $Root "docs"
$ShellFile = Join-Path $Root "tools\web\shell.html"
$ConfigFile = Join-Path $Root "tools\web\config.txt"
$DataDir = Join-Path $DocsDir "Data"
$Output = Join-Path $DocsDir "index.html"

if (-not (Test-Path $Emxx)) {
    throw "em++ was not found at $Emxx. Put the emsdk in the repository root."
}
if (-not (Test-Path $DataDir)) {
    throw "Game data was not found at $DataDir."
}
if (-not (Test-Path $ConfigFile)) {
    throw "Web config was not found at $ConfigFile."
}

Set-Location $Root

if ($Clean) {
    Remove-Item (Join-Path $DocsDir "index.html") -ErrorAction SilentlyContinue
    Remove-Item (Join-Path $DocsDir "index.js") -ErrorAction SilentlyContinue
    Remove-Item (Join-Path $DocsDir "index.wasm") -ErrorAction SilentlyContinue
    Remove-Item (Join-Path $DocsDir "index.data") -ErrorAction SilentlyContinue
}

# These are Mac OS 9 / Win32-only translation units.  The original Visual
# Studio project does not compile Alerts.cpp or AGL_DSp.cpp either.
$Excluded = @("AGL_DSp.cpp", "Alerts.cpp", "stdafx.cpp")

$Sources = Get-ChildItem -Path $SourceDir -Filter *.cpp |
    Where-Object { $Excluded -notcontains $_.Name } |
    Sort-Object Name |
    ForEach-Object { $_.FullName }

$Sources += (Join-Path $WebDir "WebAudio.cpp")
$Sources += (Join-Path $WebDir "WebGL.cpp")

$Optimization = if ($Release) { "-O2" } else { "-O1" }
$Assertions = if ($Release) { "0" } else { "1" }

$Arguments = @(
    "-std=gnu++98",
    $Optimization,
    "-g",
    "-fno-strict-aliasing",
    "-Wno-deprecated-declarations",
    "-Wno-array-bounds",

    "-DNOOGG",
    "-I$SourceDir",
    "-I$WebDir",

    "-sUSE_SDL=2",
    "-sLEGACY_GL_EMULATION=1",
    "-sGL_UNSAFE_OPTS=0",
    "-sGL_FFP_ONLY=1",
    "-sMAX_WEBGL_VERSION=1",
    "-sMIN_WEBGL_VERSION=1",
    "-sASSERTIONS=$Assertions",
    "-sALLOW_MEMORY_GROWTH=1",
    "-sINITIAL_MEMORY=67108864",
    "-sSTACK_SIZE=8388608",
    "-sENVIRONMENT=web",
    "-sFORCE_FILESYSTEM=1",
    "-sEXIT_RUNTIME=0",

    "--shell-file", $ShellFile,
    "--preload-file", "docs/Data@Data",
    "--preload-file", "tools/web/config.txt@config.txt",
    "-o", $Output
)

$Arguments += $Sources

Write-Host "Building Black Shades for the web..."
Write-Host "  compiler : $Emxx"
Write-Host "  sources  : $($Sources.Count)"
Write-Host "  output   : $Output"
Write-Host ""
Write-Host "Compiling sources with Emscripten (this can take several minutes; the window will pause silently during linking)." -ForegroundColor Yellow
Write-Host "Please wait..." -ForegroundColor Yellow

$BuildLog = Join-Path $env:TEMP 'blackshades-build.log'
$ErrorActionPreference = 'Continue'
& $Emxx @Arguments 2>&1 | Tee-Object -FilePath $BuildLog | Out-Null
$BuildExit = $LASTEXITCODE
$ErrorActionPreference = 'Stop'
if ($BuildExit -ne 0) {
    Write-Host "BUILD FAILED (exit $BuildExit)" -ForegroundColor Red
    Get-Content $BuildLog -ErrorAction SilentlyContinue | Select-Object -Last 25
    Read-Host "Press Enter to exit..."
    exit $BuildExit
}
Remove-Item $BuildLog -ErrorAction SilentlyContinue

Write-Host "BUILD SUCCESS" -ForegroundColor Green

$Outputs = @("index.html", "index.js", "index.wasm", "index.data")
foreach ($Name in $Outputs) {
    $Path = Join-Path $DocsDir $Name
    if (-not (Test-Path $Path)) {
        Write-Host "BUILD FAILED: $Path missing" -ForegroundColor Red
        Read-Host "Press Enter to exit..."
        exit 1
    }
}

Write-Host ""
Write-Host "Web build complete:"
Get-ChildItem -Path $DocsDir -File |
    Where-Object { $Outputs -contains $_.Name } |
    Select-Object Name, Length |
    Format-Table -AutoSize

Write-Host ""
Write-Host "Press Enter to close..." -ForegroundColor Cyan
Read-Host
