<#
  gothic-diablo – launcher

  1. Sprawdza na GitHubie najnowszy build (release "build-N"), pobiera go i instaluje
     do gra\wersje\build-N.
  2. Zapisy, ustawienia i logi trzyma w gra\dane – wspólne dla wszystkich wersji.
  3. Usuwa stare wersje (zostawia -Keep najnowszych, domyślnie 2).
  4. Uruchamia grę.

  Normalnie uruchamiany przez uruchom_arpg.bat / uruchom_klasycznie.bat.
  Opcje (można dopisać do .bat lub podać w wierszu poleceń):
    -Classic          tryb klasyczny (bez ARPG)
    -Offline          bez sprawdzania aktualizacji
    -Branch <nazwa>   tylko buildy z tej gałęzi (np. master)
    -Build <N>        konkretny build, np. -Build 3631 (cofnięcie wersji)
    -Keep <N>         ile wersji trzymać na dysku
    -NoStart          tylko aktualizacja, bez uruchamiania gry
    -GameArgs ...     dodatkowe parametry dla Gothic2Notr.exe
#>
[CmdletBinding()]
param(
  [switch]$Classic,
  [switch]$Offline,
  [string]$Branch = '',
  [int]$Build = 0,
  [ValidateRange(1, 50)][int]$Keep = 2,
  [switch]$NoStart,
  [string]$GothicPath = 'F:\SteamLibrary\steamapps\common\Gothic II',
  [string]$Repo = 'wr0b3l/gothic-diablo',
  [Parameter(ValueFromRemainingArguments = $true)][string[]]$GameArgs
)

$ErrorActionPreference = 'Stop'
$ProgressPreference    = 'SilentlyContinue'
[Net.ServicePointManager]::SecurityProtocol = [Net.ServicePointManager]::SecurityProtocol -bor [Net.SecurityProtocolType]::Tls12

$Root      = $PSScriptRoot
$GameRoot  = Join-Path $Root 'gra'
$Versions  = Join-Path $GameRoot 'wersje'
$DataDir   = Join-Path $GameRoot 'dane'
$ExeName   = 'Gothic2Notr.exe'
$UserAgent = 'gothic-diablo-launcher'

function Say ([string]$m)  { Write-Host $m }
function Warn([string]$m)  { Write-Host $m -ForegroundColor Yellow }
function Good([string]$m)  { Write-Host $m -ForegroundColor Green }

function Test-GameRunning { [bool](Get-Process -Name 'Gothic2Notr' -ErrorAction SilentlyContinue) }

# --- zainstalowane wersje ---------------------------------------------------

function Get-Installed {
  if (-not (Test-Path $Versions)) { return @() }
  $list = foreach ($d in Get-ChildItem $Versions -Directory) {
    if ($d.Name -notmatch '^build-(\d+)$') { continue }
    $n = [int]$Matches[1]
    if (-not (Test-Path (Join-Path $d.FullName $ExeName))) { continue }
    $info = $null
    $infoFile = Join-Path $d.FullName 'build.json'
    if (Test-Path $infoFile) {
      try { $info = Get-Content $infoFile -Raw -Encoding UTF8 | ConvertFrom-Json } catch { }
    }
    [pscustomobject]@{
      Number = $n
      Tag    = $d.Name
      Path   = $d.FullName
      Branch = if ($info) { [string]$info.Branch } else { '' }
      Title  = if ($info) { [string]$info.Title }  else { $d.Name }
    }
  }
  @($list | Sort-Object Number -Descending)
}

# --- GitHub -------------------------------------------------------------------

function Get-RemoteBuilds {
  $uri = "https://api.github.com/repos/$Repo/releases?per_page=50"
  $headers = @{ 'User-Agent' = $UserAgent; 'Accept' = 'application/vnd.github+json' }
  $releases = Invoke-RestMethod -Uri $uri -Headers $headers -TimeoutSec 20
  $list = foreach ($r in $releases) {
    if ($r.tag_name -notmatch '^build-(\d+)$') { continue }
    $n = [int]$Matches[1]
    $asset = @($r.assets | Where-Object { $_.name -like '*win64.zip' })[0]
    if (-not $asset) { continue }
    $br = ''
    if ([string]$r.body -match '(?m)^branch:\s*(\S+)') { $br = $Matches[1] }
    [pscustomobject]@{
      Number = $n
      Tag    = [string]$r.tag_name
      Branch = $br
      Title  = [string]$r.name
      Notes  = [string]$r.body
      Url    = [string]$asset.browser_download_url
      Size   = [long]$asset.size
      Digest = [string]$asset.digest
    }
  }
  @($list | Sort-Object Number -Descending)
}

function Save-Download([string]$Url, [string]$Dest, [long]$ExpectedSize) {
  Add-Type -AssemblyName System.Net.Http
  $client = New-Object System.Net.Http.HttpClient
  $client.Timeout = [TimeSpan]::FromMinutes(60)
  $client.DefaultRequestHeaders.UserAgent.ParseAdd($UserAgent)
  try {
    $resp = $client.GetAsync($Url, [System.Net.Http.HttpCompletionOption]::ResponseHeadersRead).GetAwaiter().GetResult()
    [void]$resp.EnsureSuccessStatusCode()
    $total = $resp.Content.Headers.ContentLength
    if (-not $total) { $total = $ExpectedSize }
    $in  = $resp.Content.ReadAsStreamAsync().GetAwaiter().GetResult()
    $out = [IO.File]::Create($Dest)
    try {
      $buf  = New-Object byte[] (1MB)
      $done = 0L
      $sw   = [Diagnostics.Stopwatch]::StartNew()
      $next = 0
      while (($read = $in.Read($buf, 0, $buf.Length)) -gt 0) {
        $out.Write($buf, 0, $read)
        $done += $read
        if ($sw.ElapsedMilliseconds -ge $next) {
          $next = $sw.ElapsedMilliseconds + 250
          $speed = ($done / 1MB) / [Math]::Max($sw.Elapsed.TotalSeconds, 0.1)
          if ($total) {
            $pct = [int](100 * $done / $total)
            Write-Host -NoNewline ("`r  {0,3}%  {1:N0} / {2:N0} MB  ({3:N1} MB/s)   " -f $pct, ($done / 1MB), ($total / 1MB), $speed)
          } else {
            Write-Host -NoNewline ("`r  {0:N0} MB  ({1:N1} MB/s)   " -f ($done / 1MB), $speed)
          }
        }
      }
      Write-Host -NoNewline ("`r  100%  {0:N0} MB w {1:N0} s                         " -f ($done / 1MB), $sw.Elapsed.TotalSeconds)
      Write-Host ''
    } finally {
      $out.Dispose()
      $in.Dispose()
    }
  } finally {
    $client.Dispose()
  }
}

function Install-Build($rel) {
  New-Item -ItemType Directory -Force -Path $Versions | Out-Null
  $zip     = Join-Path $Versions "$($rel.Tag).zip.part"
  $staging = Join-Path $Versions "$($rel.Tag).tmp"
  $final   = Join-Path $Versions $rel.Tag

  Say ("Pobieram {0} ({1:N0} MB)..." -f $rel.Title, ($rel.Size / 1MB))
  Save-Download $rel.Url $zip $rel.Size

  if ($rel.Size -gt 0 -and (Get-Item $zip).Length -ne $rel.Size) {
    Remove-Item $zip -Force
    throw "Pobrany plik ma zły rozmiar – spróbuj ponownie."
  }
  if ($rel.Digest -match '^sha256:([0-9a-fA-F]{64})$') {
    $want = $Matches[1]
    $have = (Get-FileHash $zip -Algorithm SHA256).Hash
    if ($have -ne $want) {
      Remove-Item $zip -Force
      throw "Suma kontrolna pobranego pliku się nie zgadza – spróbuj ponownie."
    }
  }

  Say "Rozpakowuję..."
  if (Test-Path $staging) { Remove-Item $staging -Recurse -Force }
  Add-Type -AssemblyName System.IO.Compression.FileSystem
  [IO.Compression.ZipFile]::ExtractToDirectory($zip, $staging)
  if (-not (Test-Path (Join-Path $staging $ExeName))) {
    Remove-Item $staging -Recurse -Force
    throw "W paczce nie ma $ExeName."
  }
  [pscustomobject]@{ Number = $rel.Number; Tag = $rel.Tag; Branch = $rel.Branch; Title = $rel.Title; Notes = $rel.Notes } |
    ConvertTo-Json | Set-Content (Join-Path $staging 'build.json') -Encoding UTF8

  if (Test-Path $final) { Remove-Item $final -Recurse -Force }
  Rename-Item $staging $rel.Tag
  Remove-Item $zip -Force
  Good "Zainstalowano $($rel.Title)."
}

function Show-Notes($rel) {
  $lines = ([string]$rel.Notes) -split "`r?`n"
  $body  = $lines | Where-Object { $_ -notmatch '^(branch|commit|pr):' }
  $text  = (($body -join "`n").Trim())
  if ($text) {
    Say ''
    Say 'Co nowego:'
    foreach ($l in ($text -split "`n")) { Say "  $l" }
    Say ''
  }
}

# --- porządki -----------------------------------------------------------------

# Stary układ: artefakt rozpakowany ręcznie do build\. Zapisy i ustawienia trafiają do gra\dane,
# pliki gry są usuwane. Inne pliki zostają nietknięte.
function Move-OldBuild {
  $old = Join-Path $Root 'build'
  if (-not (Test-Path (Join-Path $old $ExeName))) { return }
  if (Test-GameRunning) { return }
  Say 'Przenoszę zapisy i ustawienia ze starego folderu build do gra\dane...'
  foreach ($f in Get-ChildItem $old -File) {
    if ($f.Name -like 'save_slot_*.sav' -or $f.Name -eq 'Gothic.ini') {
      $dst = Join-Path $DataDir $f.Name
      if (Test-Path $dst) { Warn "  $($f.Name) jest już w gra\dane – zostawiam kopię w build."; continue }
      Move-Item $f.FullName $dst
    }
  }
  foreach ($f in Get-ChildItem $old -File) {
    if ($f.Extension -in '.exe', '.dll', '.bat' -or $f.Name -in 'log.txt', 'crash.log', 'VERSION') {
      Remove-Item $f.FullName -Force
    }
  }
  if (-not (Get-ChildItem $old -Force)) {
    Remove-Item $old -Force
    Say 'Usunięto stary folder build.'
  } else {
    Warn 'W folderze build zostały inne pliki – zostawiam je.'
  }
}

function Remove-OldVersions([string]$InUse) {
  if (-not (Test-Path $Versions)) { return }
  # przerwane pobieranie / rozpakowywanie
  Get-ChildItem $Versions -Force | Where-Object { $_.Name -like '*.tmp' -or $_.Name -like '*.part' } |
    ForEach-Object { try { Remove-Item $_.FullName -Recurse -Force } catch { } }

  $installed = Get-Installed
  $keepTags  = @($installed | Select-Object -First $Keep | ForEach-Object { $_.Tag })
  foreach ($v in $installed) {
    if ($keepTags -contains $v.Tag -or $v.Tag -eq $InUse) { continue }
    try {
      Remove-Item $v.Path -Recurse -Force
      Say "Usunięto starą wersję $($v.Tag)."
    } catch {
      Warn "Nie mogę usunąć $($v.Tag) (gra uruchomiona?) – spróbuję następnym razem."
    }
  }
}

# --- start --------------------------------------------------------------------

try {
  New-Item -ItemType Directory -Force -Path $Versions, $DataDir | Out-Null

  if (-not $Offline) {
    try {
      Say 'Sprawdzam aktualizacje...'
      $remote = Get-RemoteBuilds
      if ($Branch) { $remote = @($remote | Where-Object { $_.Branch -eq $Branch }) }
      if ($Build)  { $remote = @($remote | Where-Object { $_.Number -eq $Build }) }
      $latest = $remote | Select-Object -First 1
      if (-not $latest) {
        Warn 'Na GitHubie nie ma pasującego buildu.'
      } elseif (Test-Path (Join-Path (Join-Path $Versions $latest.Tag) $ExeName)) {
        Good "Masz aktualną wersję: $($latest.Title)"
      } else {
        Say "Nowa wersja: $($latest.Title)"
        Show-Notes $latest
        Install-Build $latest
      }
    } catch {
      Warn "Nie udało się sprawdzić / pobrać aktualizacji: $($_.Exception.Message)"
      Warn 'Uruchamiam wersję, która jest na dysku.'
    }
  }

  $installed = Get-Installed
  $run = $null
  if ($Build)      { $run = $installed | Where-Object { $_.Number -eq $Build }  | Select-Object -First 1 }
  elseif ($Branch) { $run = $installed | Where-Object { $_.Branch -eq $Branch } | Select-Object -First 1 }
  else             { $run = $installed | Select-Object -First 1 }

  if (-not $run) {
    $what = if ($Build) { "buildu $Build" } elseif ($Branch) { "buildu z gałęzi $Branch" } else { 'żadnej wersji gry' }
    throw "Na dysku nie ma $what, a pobranie się nie udało."
  }

  Move-OldBuild
  Remove-OldVersions $run.Tag

  if ($NoStart) { Good "Gotowe ($($run.Title))."; exit 0 }

  if (-not (Test-Path (Join-Path $GothicPath 'Data'))) {
    throw "Nie znaleziono Gothic II w '$GothicPath'. Popraw -GothicPath w launcher.ps1."
  }
  if (Test-GameRunning) { Warn 'Gra już działa.'; exit 0 }

  $gameArgList = @('-g', "`"$GothicPath`"")
  if ($Classic)  { $gameArgList += '-classic' }
  if ($GameArgs) { $gameArgList += $GameArgs }

  $mode = if ($Classic) { 'klasycznie' } else { 'ARPG' }
  Say "Uruchamiam $($run.Title) ($mode)..."
  Start-Process -FilePath (Join-Path $run.Path $ExeName) -ArgumentList $gameArgList -WorkingDirectory $DataDir
  exit 0
} catch {
  Write-Host ''
  Write-Host "BŁĄD: $($_.Exception.Message)" -ForegroundColor Red
  exit 1
}
