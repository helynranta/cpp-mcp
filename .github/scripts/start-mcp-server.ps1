$ErrorActionPreference = "Stop"

$serverExePath = "./build/ci-windows/examples/Release/server_example.exe"
$stdoutLogPath = "server-stdout.log"
$stderrLogPath = "server-stderr.log"
$hostName = "127.0.0.1"
$port = 3001

Write-Host "Checking for server binary..."
if (-not (Test-Path "./build/ci-windows/examples/Release")) {
  Write-Host "❌ Build directory not found"
  Write-Host "Available build directories:"
  Get-ChildItem -Path "./build/" -ErrorAction SilentlyContinue
  exit 1
}

if (-not (Test-Path $serverExePath)) {
  Write-Host "❌ server_example.exe binary not found"
  exit 1
}

Write-Host "✅ server_example.exe binary found"

$process = Start-Process -FilePath $serverExePath `
  -ArgumentList "--host", $hostName, "--port", "$port" `
  -RedirectStandardOutput $stdoutLogPath `
  -RedirectStandardError $stderrLogPath `
  -NoNewWindow `
  -PassThru

$serverPid = $process.Id
"SERVER_PID=$serverPid" | Out-File -FilePath $env:GITHUB_ENV -Append
Write-Host "Started server with PID: $serverPid"

Start-Sleep -Seconds 2

Write-Host "Waiting for server to start..."
for ($i = 1; $i -le 60; $i++) {
  if (-not (Get-Process -Id $serverPid -ErrorAction SilentlyContinue)) {
    Write-Host "❌ Server process has died"
    Write-Host "=== Server stdout ==="
    Get-Content $stdoutLogPath -ErrorAction SilentlyContinue
    Write-Host "=== Server stderr ==="
    Get-Content $stderrLogPath -ErrorAction SilentlyContinue
    exit 1
  }

  $tcpClient = [System.Net.Sockets.TcpClient]::new()
  try {
    $connectTask = $tcpClient.ConnectAsync($hostName, $port)
    if ($connectTask.Wait(500)) {
      Write-Host "✅ Server port is open and responding"
      Write-Host "=== Last 10 lines from stdout ==="
      Get-Content $stdoutLogPath -Tail 10 -ErrorAction SilentlyContinue
      Write-Host "=== Last 10 lines from stderr ==="
      Get-Content $stderrLogPath -Tail 10 -ErrorAction SilentlyContinue
      exit 0
    }
  } catch {
    # Connection not ready yet.
  } finally {
    $tcpClient.Dispose()
  }

  Start-Sleep -Milliseconds 500
}

Write-Host "❌ Server failed to start within 30 seconds"
Write-Host "Checking if server process is still running..."
if (Get-Process -Id $serverPid -ErrorAction SilentlyContinue) {
  Write-Host "⚠️  Server process is running but not responding"
} else {
  Write-Host "❌ Server process has died"
}
Write-Host ""
Write-Host "=== Full Server stdout ==="
Get-Content $stdoutLogPath -ErrorAction SilentlyContinue
Write-Host ""
Write-Host "=== Full Server stderr ==="
Get-Content $stderrLogPath -ErrorAction SilentlyContinue
exit 1
