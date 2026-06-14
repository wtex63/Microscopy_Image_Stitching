param(
    [Parameter(Mandatory = $true)][string]$CpuLog,
    [Parameter(Mandatory = $true)][string]$GpuLog,
    [Parameter(Mandatory = $true)][string]$Image1,
    [Parameter(Mandatory = $true)][string]$Image2,
    [Parameter(Mandatory = $true)][string]$CsvPath
)

function Get-LogMetrics {
    param(
        [Parameter(Mandatory = $true)][string]$Path,
        [Parameter(Mandatory = $true)][string]$Mode,
        [Parameter(Mandatory = $true)][string]$Image1,
        [Parameter(Mandatory = $true)][string]$Image2
    )

    $lines = Get-Content -LiteralPath $Path -ErrorAction Stop
    $backendLine = ($lines | Select-String 'Phase backend:' | Select-Object -Last 1).Line
    $timingLine = ($lines | Select-String 'Pair timing summary:' | Select-Object -Last 1).Line
    $breakdownLine = ($lines | Select-String 'Fallback breakdown:' | Select-Object -Last 1).Line

    $backend = ''
    if ($backendLine -match 'Phase backend:\s*(.+)$') { $backend = $Matches[1].Trim() }

    $fft1 = ''; $fft2 = ''; $phase = ''; $fallback = ''; $blend = ''
    if ($timingLine -match 'FFT1=([^,]+), FFT2=([^,]+), Phase=([^,]+), Fallback=([^,]+), Blend=(.+)$') {
        $fft1 = $Matches[1].Trim()
        $fft2 = $Matches[2].Trim()
        $phase = $Matches[3].Trim()
        $fallback = $Matches[4].Trim()
        $blend = $Matches[5].Trim()
    }

    $local = ''; $global = ''; $edge = ''
    if ($breakdownLine -match 'local=([^,]+), global=([^,]+), edge/refine=(.+)$') {
        $local = $Matches[1].Trim()
        $global = $Matches[2].Trim()
        $edge = $Matches[3].Trim()
    }

    [pscustomobject]@{
        timestamp = Get-Date -Format 'yyyy-MM-dd HH:mm:ss'
        mode = $Mode
        backend = $backend
        image1 = $Image1
        image2 = $Image2
        fft1 = $fft1
        fft2 = $fft2
        phase = $phase
        fallback = $fallback
        blend = $blend
        fallback_local = $local
        fallback_global = $global
        fallback_edge_refine = $edge
        log_path = $Path
    }
}

$rows = @(
    Get-LogMetrics -Path $CpuLog -Mode 'CPU' -Image1 $Image1 -Image2 $Image2
    Get-LogMetrics -Path $GpuLog -Mode 'GPU' -Image1 $Image1 -Image2 $Image2
)

$dir = Split-Path -Parent $CsvPath
if (-not [string]::IsNullOrWhiteSpace($dir) -and -not (Test-Path -LiteralPath $dir)) {
    New-Item -ItemType Directory -Path $dir | Out-Null
}

if (Test-Path -LiteralPath $CsvPath) {
    $rows | Export-Csv -LiteralPath $CsvPath -Append -NoTypeInformation
}
else {
    $rows | Export-Csv -LiteralPath $CsvPath -NoTypeInformation
}
