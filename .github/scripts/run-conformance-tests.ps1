<#
.SYNOPSIS
    Starts the MCP server, builds the conformance tool from source, runs tests,
    then stops the server.  Designed to be run from the repository root.

.DESCRIPTION
    Everything happens in a single PowerShell invocation so the server process
    lifetime is fully contained within this step.  This avoids the Windows
    cross-step background-process instability seen when the server is started in
    one step and tested in another.

    Steps performed:
      1. Locate server_example.exe (explicit path or auto-detect from build tree)
      2. Start the server in the background, redirecting stdout/stderr to log files
      3. Wait up to 30 s for the server to accept TCP connections
      4. Clone and build @modelcontextprotocol/conformance from the git tag
         (output is cached in $env:TEMP\mcp-conformance-<tag> across runs)
      5. Run: node <cli> server --url <url> --suite active [--expected-failures ...]
      6. Print the last 30 lines of each server log file
      7. Stop the server
      8. Exit with the conformance tool's exit code

.PARAMETER ServerBinary
    Explicit path to server_example.exe.  If omitted the script searches the
    common CMake output locations under ./build.

.PARAMETER ServerHost
    Hostname the server should bind to.  Default: 127.0.0.1

.PARAMETER ServerPort
    Port the server should listen on.  Default: 3001

.PARAMETER ExpectedFailures
    Path to the expected-failures YAML baseline.  Passed to the conformance
    CLI only when the file exists.  Default: ./conformance-baseline.yml

.PARAMETER ConformanceTag
    Git tag of modelcontextprotocol/conformance to clone and build.
    Default: v0.1.14

.EXAMPLE
    # Run from the repository root after building server_example:
    .\.github\scripts\run-conformance-tests.ps1

    # Explicit binary path:
    .\.github\scripts\run-conformance-tests.ps1 `
        -ServerBinary .\build\ci-windows\examples\server_example.exe
#>
param(
    [string] $ServerBinary     = "",
    [string] $ServerHost       = "127.0.0.1",
    [int]    $ServerPort       = 3001,
    [string] $ExpectedFailures = "./conformance-baseline.yml",
    [string] $ConformanceTag   = "v0.1.14"
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

$stdoutLog      = "server-stdout.log"
$stderrLog      = "server-stderr.log"
$serverProcess  = $null
$conformanceDir = Join-Path $env:TEMP "mcp-conformance-$ConformanceTag"
$testExitCode   = 1   # pessimistic default

# ---------------------------------------------------------------------------
function Write-Section([string]$title) {
    Write-Host ""
    Write-Host "=" * 60
    Write-Host "  $title"
    Write-Host "=" * 60
}

function Stop-McpServer {
    if ($null -ne $serverProcess -and -not $serverProcess.HasExited) {
        Write-Host "Stopping server (PID $($serverProcess.Id))..."
        try {
            Stop-Process -Id $serverProcess.Id -Force -ErrorAction SilentlyContinue
        } catch { }
        Write-Host "Server stopped."
    }
}

# ---------------------------------------------------------------------------
# STEP 1 – locate server binary
# ---------------------------------------------------------------------------
Write-Section "Step 1: Locate server binary"

$candidates = @(
    $ServerBinary,
    "./build/ci-windows/examples/server_example.exe",
    "./build/ci-windows/examples/Release/server_example.exe",
    "./build/examples/server_example.exe",
    "./build/examples/Release/server_example.exe"
) | Where-Object { $_ -ne "" }

$resolvedBinary = $null
foreach ($c in $candidates) {
    if (Test-Path $c) { $resolvedBinary = $c; break }
}

if (-not $resolvedBinary) {
    Write-Host "ERROR: server_example.exe not found.  Searched:"
    $candidates | ForEach-Object { Write-Host "  $_" }
    exit 1
}
Write-Host "OK: $resolvedBinary"

# ---------------------------------------------------------------------------
# STEP 2 – start server
# ---------------------------------------------------------------------------
Write-Section "Step 2: Start server"

# Remove stale log files so later reads reflect only this run
Remove-Item $stdoutLog -ErrorAction SilentlyContinue
Remove-Item $stderrLog -ErrorAction SilentlyContinue

$serverProcess = Start-Process `
    -FilePath    $resolvedBinary `
    -ArgumentList "--host", $ServerHost, "--port", "$ServerPort" `
    -RedirectStandardOutput $stdoutLog `
    -RedirectStandardError  $stderrLog `
    -NoNewWindow `
    -PassThru

Write-Host "Server PID: $($serverProcess.Id)"

# Publish PID for the safety-net stop step in the workflow (if running in CI)
if ($env:GITHUB_ENV) {
    "SERVER_PID=$($serverProcess.Id)" | Out-File -FilePath $env:GITHUB_ENV -Append
}

# ---------------------------------------------------------------------------
# STEP 3 – wait for server to accept TCP connections
# ---------------------------------------------------------------------------
Write-Section "Step 3: Wait for server ready (TCP $ServerHost`:$ServerPort)"

Start-Sleep -Seconds 2

$ready = $false
for ($attempt = 1; $attempt -le 60; $attempt++) {

    if ($serverProcess.HasExited) {
        Write-Host "ERROR: Server process exited unexpectedly (code $($serverProcess.ExitCode))"
        Write-Host "--- stdout ---"
        Get-Content $stdoutLog -ErrorAction SilentlyContinue
        Write-Host "--- stderr ---"
        Get-Content $stderrLog -ErrorAction SilentlyContinue
        exit 1
    }

    $tcp = [System.Net.Sockets.TcpClient]::new()
    try {
        $connectTask = $tcp.ConnectAsync($ServerHost, $ServerPort)
        if ($connectTask.Wait(500)) {
            $ready = $true
            break
        }
    } catch { }
    finally { $tcp.Dispose() }

    Start-Sleep -Milliseconds 500
}

if (-not $ready) {
    Write-Host "ERROR: Server did not open TCP port within 30 s"
    Write-Host "--- stdout ---"
    Get-Content $stdoutLog -ErrorAction SilentlyContinue
    Write-Host "--- stderr ---"
    Get-Content $stderrLog -ErrorAction SilentlyContinue
    Stop-McpServer
    exit 1
}
Write-Host "OK: server is accepting connections"

# ---------------------------------------------------------------------------
# STEP 4 – build conformance tool from git source
# ---------------------------------------------------------------------------
Write-Section "Step 4: Build conformance tool ($ConformanceTag)"

$cliPath = Join-Path $conformanceDir "dist\index.js"

if (Test-Path $cliPath) {
    Write-Host "Reusing cached build: $cliPath"
} else {
    Write-Host "Cloning modelcontextprotocol/conformance $ConformanceTag ..."
    if (Test-Path $conformanceDir) {
        Remove-Item -Recurse -Force $conformanceDir
    }

    git clone --depth 1 --branch $ConformanceTag `
        https://github.com/modelcontextprotocol/conformance.git `
        $conformanceDir

    Push-Location $conformanceDir
    try {
        Write-Host "Running npm ci ..."
        npm ci
        Write-Host "Running npm run build ..."
        npm run build
    } finally {
        Pop-Location
    }
}

if (-not (Test-Path $cliPath)) {
    Write-Host "ERROR: CLI not found after build: $cliPath"
    Stop-McpServer
    exit 1
}
Write-Host "OK: $cliPath"

# ---------------------------------------------------------------------------
# STEP 5 – run conformance tests
# ---------------------------------------------------------------------------
Write-Section "Step 5: Run conformance tests"

$runArgs = [System.Collections.Generic.List[string]]::new()
$runArgs.Add($cliPath)
$runArgs.Add("server")
$runArgs.Add("--url")
$runArgs.Add("http://${ServerHost}:${ServerPort}/mcp")
$runArgs.Add("--suite")
$runArgs.Add("active")
$runArgs.Add("--verbose")

if ($ExpectedFailures -and (Test-Path $ExpectedFailures)) {
    $absBaseline = (Resolve-Path $ExpectedFailures).Path
    $runArgs.Add("--expected-failures")
    $runArgs.Add($absBaseline)
    Write-Host "Using baseline: $absBaseline"
} else {
    Write-Host "WARNING: No expected-failures file at '$ExpectedFailures'; running without baseline"
}

Write-Host "Command: node $($runArgs -join ' ')"
Write-Host ""

try {
    & node @runArgs
    $testExitCode = $LASTEXITCODE
} catch {
    Write-Host "ERROR launching node: $_"
    $testExitCode = 1
}

# ---------------------------------------------------------------------------
# STEP 6 – show server logs and stop server
# ---------------------------------------------------------------------------
Write-Section "Step 6: Server logs (last 30 lines each)"
Write-Host "--- stdout ---"
Get-Content $stdoutLog -Tail 30 -ErrorAction SilentlyContinue
Write-Host "--- stderr ---"
Get-Content $stderrLog -Tail 30 -ErrorAction SilentlyContinue

Stop-McpServer

# ---------------------------------------------------------------------------
Write-Host ""
if ($testExitCode -eq 0) {
    Write-Host "RESULT: All conformance tests passed (or only expected failures)"
} else {
    Write-Host "RESULT: Conformance tests FAILED (exit code $testExitCode)"
}

exit $testExitCode
