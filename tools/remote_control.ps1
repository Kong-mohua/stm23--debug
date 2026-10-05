param([Parameter(Mandatory=$true)][string]$Port, [int]$BaudRate = 9600)
$ErrorActionPreference = 'Stop'
$serial = [System.IO.Ports.SerialPort]::new($Port, $BaudRate, 'None', 8, 'One')
$serial.NewLine = "`n"
$serial.ReadTimeout = 100
$serial.WriteTimeout = 250
$serial.DtrEnable = $false
$serial.RtsEnable = $false
$motion = 'STOP'
$watch = [Diagnostics.Stopwatch]::StartNew()
$lastSend = -300L
try {
    $serial.Open()
    Write-Host 'W:forward S:back A:left D:right Space:stop Q:quit'
    Write-Host 'Direction remains selected until changed. Keep this window focused.'
    $running = $true
    while ($running) {
        if ([Console]::KeyAvailable) {
            $key = [Console]::ReadKey($true).Key
            switch ($key) {
                W { $motion = 'FORWARD' }
                S { $motion = 'BACK' }
                A { $motion = 'LEFT' }
                D { $motion = 'RIGHT' }
                Spacebar { $motion = 'STOP' }
                Q { $motion = 'STOP'; $running = $false }
            }
            $lastSend = -300L
        }
        if ($watch.ElapsedMilliseconds - $lastSend -ge 300) {
            $serial.WriteLine("MOVE $motion")
            $lastSend = $watch.ElapsedMilliseconds
        }
        if ($serial.BytesToRead -gt 0) { Write-Host -NoNewline $serial.ReadExisting() }
        Start-Sleep -Milliseconds 20
    }
} finally {
    if ($serial.IsOpen) {
        try { $serial.WriteLine('STOP') } catch { }
        $serial.Close()
    }
    $serial.Dispose()
}
