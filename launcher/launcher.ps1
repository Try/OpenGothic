<#
  gothic-diablo – launcher

  1. Sprawdza na GitHubie najnowszy build (release "build-N"), pobiera go i instaluje
     do gra\wersje\build-N.
  2. Zapisy, ustawienia i logi trzyma w gra\dane – wspólne dla wszystkich wersji.
  3. Usuwa stare wersje (zostawia -Keep najnowszych, domyślnie 2).
  4. Uruchamia grę.

  Bez parametrów otwiera okno launchera ("Gothic Diablo.lnk" / "Gothic Diablo.bat").
  -Console: bez okna, od razu aktualizacja + gra (uruchom_arpg.bat / uruchom_klasycznie.bat).

  Opcje:
    -Classic          tryb klasyczny (bez ARPG) – tylko z -Console
    -Offline          bez sprawdzania aktualizacji
    -Branch <nazwa>   tylko buildy z tej gałęzi (np. master)
    -Build <N>        konkretny build, np. -Build 3631 (cofnięcie wersji)
    -Keep <N>         ile wersji trzymać na dysku
    -NoStart          tylko aktualizacja, bez uruchamiania gry
    -GothicPath ...   folder Gothic II
    pozostałe parametry trafiają do Gothic2Notr.exe
#>
[CmdletBinding()]
param(
  [switch]$Console,
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

$Cfg = @{
  Root       = $PSScriptRoot
  Versions   = Join-Path $PSScriptRoot 'gra\wersje'
  DataDir    = Join-Path $PSScriptRoot 'gra\dane'
  Exe        = 'Gothic2Notr.exe'
  UserAgent  = 'gothic-diablo-launcher'
  Repo       = $Repo
  GothicPath = $GothicPath
  Keep       = $Keep
  GameArgs   = @($GameArgs | Where-Object { $_ })
}

# =============================================================================
# Logika wspólna dla konsoli i okna. W oknie wykonywana także w osobnym wątku
# (runspace), dlatego wszystko zależy tylko od $Cfg i opcjonalnego $Sync.
# =============================================================================
$Core = {
  function Log([string]$Msg, [string]$Level = 'info') {
    if ($Sync) { $Sync.Status = $Msg; $Sync.Level = $Level; return }
    $color = switch ($Level) { 'warn' { 'Yellow' } 'ok' { 'Green' } 'error' { 'Red' } default { 'Gray' } }
    Write-Host $Msg -ForegroundColor $color
  }

  function Test-GameRunning { [bool](Get-Process -Name 'Gothic2Notr' -ErrorAction SilentlyContinue) }

  function Get-NotesText([string]$Notes) {
    $lines = $Notes -split "`r?`n" | Where-Object { $_ -notmatch '^(branch|commit|pr|Co-Authored-By|Claude-Session|Signed-off-by):' }
    ($lines -join "`n").Trim()
  }

  function Get-Installed {
    if (-not (Test-Path $Cfg.Versions)) { return @() }
    $list = foreach ($d in Get-ChildItem $Cfg.Versions -Directory) {
      if ($d.Name -notmatch '^build-(\d+)$') { continue }
      $n = [int]$Matches[1]
      if (-not (Test-Path (Join-Path $d.FullName $Cfg.Exe))) { continue }
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
        Title  = if ($info) { [string]$info.Title }  else { "#$n" }
        Notes  = if ($info) { [string]$info.Notes }  else { '' }
      }
    }
    @($list | Sort-Object Number -Descending)
  }

  function Get-RemoteBuilds {
    $uri = "https://api.github.com/repos/$($Cfg.Repo)/releases?per_page=50"
    $headers = @{ 'User-Agent' = $Cfg.UserAgent; 'Accept' = 'application/vnd.github+json' }
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
        Page   = [string]$r.html_url
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
    $client.DefaultRequestHeaders.UserAgent.ParseAdd($Cfg.UserAgent)
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
            $next  = $sw.ElapsedMilliseconds + 200
            $speed = ($done / 1MB) / [Math]::Max($sw.Elapsed.TotalSeconds, 0.1)
            if ($Sync) {
              $Sync.Done = $done; $Sync.Total = [long]$total; $Sync.Speed = $speed
            } elseif ($total) {
              Write-Host -NoNewline ("`r  {0,3}%  {1:N0} / {2:N0} MB  ({3:N1} MB/s)   " -f [int](100 * $done / $total), ($done / 1MB), ($total / 1MB), $speed)
            }
          }
        }
        if ($Sync) { $Sync.Done = $done; $Sync.Total = [long]$total }
        else { Write-Host ("`r  100%  {0:N0} MB w {1:N0} s                          " -f ($done / 1MB), $sw.Elapsed.TotalSeconds) }
      } finally {
        $out.Dispose()
        $in.Dispose()
      }
    } finally {
      $client.Dispose()
    }
  }

  function Install-Build($Rel) {
    New-Item -ItemType Directory -Force -Path $Cfg.Versions | Out-Null
    $zip     = Join-Path $Cfg.Versions "$($Rel.Tag).zip.part"
    $staging = Join-Path $Cfg.Versions "$($Rel.Tag).tmp"
    $final   = Join-Path $Cfg.Versions $Rel.Tag

    if ($Sync) { $Sync.Phase = 'download'; $Sync.Done = 0; $Sync.Total = $Rel.Size }
    Log ("Pobieram {0} ({1:N0} MB)..." -f $Rel.Title, ($Rel.Size / 1MB))
    Save-Download $Rel.Url $zip $Rel.Size

    if ($Rel.Size -gt 0 -and (Get-Item $zip).Length -ne $Rel.Size) {
      Remove-Item $zip -Force
      throw 'Pobrany plik ma zły rozmiar – spróbuj ponownie.'
    }
    if ($Rel.Digest -match '^sha256:([0-9a-fA-F]{64})$') {
      $want = $Matches[1]
      if ($Sync) { $Sync.Phase = 'extract' }
      Log 'Sprawdzam sumę kontrolną...'
      $fs = [IO.File]::OpenRead($zip)
      try { $have = [BitConverter]::ToString([Security.Cryptography.SHA256]::Create().ComputeHash($fs)) -replace '-', '' }
      finally { $fs.Dispose() }
      if ($have -ne $want) {
        Remove-Item $zip -Force
        throw 'Suma kontrolna pobranego pliku się nie zgadza – spróbuj ponownie.'
      }
    }

    if ($Sync) { $Sync.Phase = 'extract' }
    Log 'Rozpakowuję...'
    if (Test-Path $staging) { Remove-Item $staging -Recurse -Force }
    Add-Type -AssemblyName System.IO.Compression.FileSystem
    [IO.Compression.ZipFile]::ExtractToDirectory($zip, $staging)
    if (-not (Test-Path (Join-Path $staging $Cfg.Exe))) {
      Remove-Item $staging -Recurse -Force
      throw "W paczce nie ma $($Cfg.Exe)."
    }
    [pscustomobject]@{ Number = $Rel.Number; Tag = $Rel.Tag; Branch = $Rel.Branch; Title = $Rel.Title; Notes = $Rel.Notes } |
      ConvertTo-Json | Set-Content (Join-Path $staging 'build.json') -Encoding UTF8

    if (Test-Path $final) { Remove-Item $final -Recurse -Force }
    Rename-Item $staging $Rel.Tag
    Remove-Item $zip -Force
    if ($Sync) { $Sync.Phase = '' }
    Log "Zainstalowano $($Rel.Title)." 'ok'
  }

  # Stary układ: artefakt rozpakowany ręcznie do build\. Zapisy i Gothic.ini idą do gra\dane,
  # pliki gry są usuwane, inne pliki zostają.
  function Move-OldBuild {
    $old = Join-Path $Cfg.Root 'build'
    if (-not (Test-Path (Join-Path $old $Cfg.Exe))) { return }
    if (Test-GameRunning) { return }
    New-Item -ItemType Directory -Force -Path $Cfg.DataDir | Out-Null
    foreach ($f in Get-ChildItem $old -File) {
      if ($f.Name -like 'save_slot_*.sav' -or $f.Name -eq 'Gothic.ini') {
        $dst = Join-Path $Cfg.DataDir $f.Name
        if (Test-Path $dst) { continue }
        Move-Item $f.FullName $dst
      }
    }
    foreach ($f in Get-ChildItem $old -File) {
      if ($f.Extension -in '.exe', '.dll', '.bat' -or $f.Name -in 'log.txt', 'crash.log', 'VERSION') {
        Remove-Item $f.FullName -Force
      }
    }
    if (-not (Get-ChildItem $old -Force)) { Remove-Item $old -Force }
    Log 'Zapisy i ustawienia ze starego folderu build przeniesione do gra\dane.'
  }

  function Remove-OldVersions([string]$InUse) {
    if (-not (Test-Path $Cfg.Versions)) { return }
    Get-ChildItem $Cfg.Versions -Force | Where-Object { $_.Name -like '*.tmp' -or $_.Name -like '*.part' } |
      ForEach-Object { try { Remove-Item $_.FullName -Recurse -Force } catch { } }

    $installed = Get-Installed
    $keepTags  = @($installed | Select-Object -First $Cfg.Keep | ForEach-Object { $_.Tag })
    foreach ($v in $installed) {
      if ($keepTags -contains $v.Tag -or $v.Tag -eq $InUse) { continue }
      try { Remove-Item $v.Path -Recurse -Force } catch { }
    }
  }

  function Start-Game($Run, [bool]$ClassicMode) {
    if (-not (Test-Path (Join-Path $Cfg.GothicPath 'Data'))) {
      throw "Nie znaleziono Gothic II w '$($Cfg.GothicPath)'. Popraw -GothicPath w launcher.ps1."
    }
    New-Item -ItemType Directory -Force -Path $Cfg.DataDir | Out-Null
    $argList = @('-g', "`"$($Cfg.GothicPath)`"")
    if ($ClassicMode) { $argList += '-classic' }
    if ($Cfg.GameArgs) { $argList += $Cfg.GameArgs }
    Start-Process -FilePath (Join-Path $Run.Path $Cfg.Exe) -ArgumentList $argList -WorkingDirectory $Cfg.DataDir
  }
}
. $Core

# =============================================================================
# Tryb konsolowy
# =============================================================================
function Invoke-Console {
  New-Item -ItemType Directory -Force -Path $Cfg.Versions, $Cfg.DataDir | Out-Null

  if (-not $Offline) {
    try {
      Log 'Sprawdzam aktualizacje...'
      $remote = Get-RemoteBuilds
      if ($Branch) { $remote = @($remote | Where-Object { $_.Branch -eq $Branch }) }
      if ($Build)  { $remote = @($remote | Where-Object { $_.Number -eq $Build }) }
      $latest = $remote | Select-Object -First 1
      if (-not $latest) {
        Log 'Na GitHubie nie ma pasującego buildu.' 'warn'
      } elseif (Test-Path (Join-Path (Join-Path $Cfg.Versions $latest.Tag) $Cfg.Exe)) {
        Log "Masz aktualną wersję: $($latest.Title)" 'ok'
      } else {
        Log "Nowa wersja: $($latest.Title)"
        $text = Get-NotesText $latest.Notes
        if ($text) { Write-Host ''; Write-Host 'Co nowego:'; foreach ($l in $text -split "`n") { Write-Host "  $l" }; Write-Host '' }
        Install-Build $latest
      }
    } catch {
      Log "Nie udało się sprawdzić / pobrać aktualizacji: $($_.Exception.Message)" 'warn'
      Log 'Uruchamiam wersję, która jest na dysku.' 'warn'
    }
  }

  $installed = Get-Installed
  if ($Build)      { $run = $installed | Where-Object { $_.Number -eq $Build }  | Select-Object -First 1 }
  elseif ($Branch) { $run = $installed | Where-Object { $_.Branch -eq $Branch } | Select-Object -First 1 }
  else             { $run = $installed | Select-Object -First 1 }
  if (-not $run) {
    $what = if ($Build) { "buildu $Build" } elseif ($Branch) { "buildu z gałęzi $Branch" } else { 'żadnej wersji gry' }
    throw "Na dysku nie ma $what, a pobranie się nie udało."
  }

  Move-OldBuild
  Remove-OldVersions $run.Tag

  if ($NoStart) { Log "Gotowe ($($run.Title))." 'ok'; return }
  if (Test-GameRunning) { Log 'Gra już działa.' 'warn'; return }
  $mode = if ($Classic) { 'klasycznie' } else { 'ARPG' }
  Log "Uruchamiam $($run.Title) ($mode)..."
  Start-Game $run $Classic.IsPresent
}

# =============================================================================
# Okno
# =============================================================================
function New-LauncherShortcut {
  $lnk = Join-Path $Cfg.Root 'Gothic Diablo.lnk'
  if (Test-Path $lnk) { return }
  try {
    $shell = New-Object -ComObject WScript.Shell
    $s = $shell.CreateShortcut($lnk)
    $s.TargetPath       = (Get-Process -Id $PID).Path
    $s.Arguments        = "-NoProfile -ExecutionPolicy Bypass -WindowStyle Hidden -File `"$PSCommandPath`""
    $s.WorkingDirectory = $Cfg.Root
    $s.Description      = 'Gothic Diablo – aktualizacja i uruchamianie gry'
    $ico = Join-Path $Cfg.GothicPath 'System\Gothic2.exe'
    if (Test-Path $ico) { $s.IconLocation = "$ico,0" }
    $s.Save()
  } catch { }
}

function Invoke-Gui {
  Add-Type -AssemblyName System.Windows.Forms, System.Drawing
  try {
    Add-Type -Namespace GothicDiablo -Name Native -MemberDefinition '[DllImport("user32.dll")] public static extern bool SetProcessDPIAware();'
    [void][GothicDiablo.Native]::SetProcessDPIAware()
  } catch { }
  [Windows.Forms.Application]::EnableVisualStyles()
  New-LauncherShortcut

  $Sync = [hashtable]::Synchronized(@{ Status = ''; Level = 'info'; Phase = ''; Done = 0L; Total = 0L; Speed = 0.0; Error = $null; Result = $null })
  $St   = @{ Job = $null; Pending = $null; Items = @(); Remote = @(); LastStatus = $null; Ready = $false }

  $C = @{
    Bg     = [Drawing.Color]::FromArgb(23, 19, 15)
    Panel  = [Drawing.Color]::FromArgb(36, 29, 23)
    Line   = [Drawing.Color]::FromArgb(74, 59, 44)
    Text   = [Drawing.Color]::FromArgb(233, 223, 204)
    Muted  = [Drawing.Color]::FromArgb(168, 154, 133)
    Accent = [Drawing.Color]::FromArgb(209, 160, 84)
    Ok     = [Drawing.Color]::FromArgb(143, 191, 106)
    Warn   = [Drawing.Color]::FromArgb(224, 176, 80)
    Err    = [Drawing.Color]::FromArgb(224, 106, 90)
  }
  function Pad($l, $t, $r, $b) { New-Object Windows.Forms.Padding($l, $t, $r, $b) }

  # --- kontrolki ---
  $form = New-Object Windows.Forms.Form
  $form.Text = 'Gothic Diablo'
  $form.AutoScaleDimensions = New-Object Drawing.SizeF(96, 96)
  $form.AutoScaleMode = 'Dpi'
  $form.ClientSize  = New-Object Drawing.Size(580, 450)
  $form.MinimumSize = New-Object Drawing.Size(480, 380)
  $form.StartPosition = 'CenterScreen'
  $form.BackColor = $C.Bg
  $form.ForeColor = $C.Text
  $form.Font = New-Object Drawing.Font('Segoe UI', 9.75)
  try {
    $ico = Join-Path $Cfg.GothicPath 'System\Gothic2.exe'
    if (Test-Path $ico) { $form.Icon = [Drawing.Icon]::ExtractAssociatedIcon($ico) }
  } catch { }

  $grid = New-Object Windows.Forms.TableLayoutPanel
  $grid.Dock = 'Fill'
  $grid.Padding = Pad 20 14 20 16
  $grid.ColumnCount = 1
  [void]$grid.ColumnStyles.Add((New-Object Windows.Forms.ColumnStyle([Windows.Forms.SizeType]::Percent, 100)))
  foreach ($i in 0..6) {
    $style = if ($i -eq 4) { New-Object Windows.Forms.RowStyle([Windows.Forms.SizeType]::Percent, 100) } else { New-Object Windows.Forms.RowStyle([Windows.Forms.SizeType]::AutoSize) }
    [void]$grid.RowStyles.Add($style)
  }
  $form.Controls.Add($grid)

  $title = New-Object Windows.Forms.Label
  $title.Text = 'GOTHIC DIABLO'
  $title.Font = New-Object Drawing.Font('Georgia', 20, [Drawing.FontStyle]::Bold)
  $title.ForeColor = $C.Accent
  $title.AutoSize = $true
  $title.Margin = Pad 0 0 0 0
  $grid.Controls.Add($title, 0, 0)

  $status = New-Object Windows.Forms.Label
  $status.Text = 'Sprawdzam aktualizacje...'
  $status.ForeColor = $C.Muted
  $status.AutoSize = $true
  $status.Margin = Pad 2 2 0 12
  $grid.Controls.Add($status, 0, 1)

  $verRow = New-Object Windows.Forms.TableLayoutPanel
  $verRow.Dock = 'Fill'; $verRow.AutoSize = $true; $verRow.ColumnCount = 2; $verRow.RowCount = 1; $verRow.Margin = Pad 0 0 0 10
  [void]$verRow.ColumnStyles.Add((New-Object Windows.Forms.ColumnStyle([Windows.Forms.SizeType]::AutoSize)))
  [void]$verRow.ColumnStyles.Add((New-Object Windows.Forms.ColumnStyle([Windows.Forms.SizeType]::Percent, 100)))
  $verLabel = New-Object Windows.Forms.Label
  $verLabel.Text = 'Wersja:'; $verLabel.AutoSize = $true; $verLabel.Anchor = 'Left'; $verLabel.Margin = Pad 2 0 8 0
  $combo = New-Object Windows.Forms.ComboBox
  $combo.DropDownStyle = 'DropDownList'; $combo.FlatStyle = 'Flat'; $combo.Dock = 'Fill'
  $combo.BackColor = $C.Panel; $combo.ForeColor = $C.Text; $combo.Margin = Pad 0 0 0 0
  $verRow.Controls.Add($verLabel, 0, 0)
  $verRow.Controls.Add($combo, 1, 0)
  $grid.Controls.Add($verRow, 0, 2)

  $notesLabel = New-Object Windows.Forms.Label
  $notesLabel.Text = 'Co nowego'; $notesLabel.ForeColor = $C.Muted; $notesLabel.AutoSize = $true; $notesLabel.Margin = Pad 2 0 0 4
  $grid.Controls.Add($notesLabel, 0, 3)

  $notes = New-Object Windows.Forms.TextBox
  $notes.Multiline = $true; $notes.ReadOnly = $true; $notes.WordWrap = $true; $notes.ScrollBars = 'Vertical'
  $notes.BorderStyle = 'FixedSingle'; $notes.Dock = 'Fill'; $notes.TabStop = $false
  $notes.BackColor = $C.Panel; $notes.ForeColor = $C.Text; $notes.Margin = Pad 0 0 0 12
  $grid.Controls.Add($notes, 0, 4)

  $progRow = New-Object Windows.Forms.TableLayoutPanel
  $progRow.Dock = 'Fill'; $progRow.AutoSize = $true; $progRow.ColumnCount = 2; $progRow.RowCount = 1; $progRow.Margin = Pad 0 0 0 12
  [void]$progRow.ColumnStyles.Add((New-Object Windows.Forms.ColumnStyle([Windows.Forms.SizeType]::Percent, 100)))
  [void]$progRow.ColumnStyles.Add((New-Object Windows.Forms.ColumnStyle([Windows.Forms.SizeType]::AutoSize)))
  $bar = New-Object Windows.Forms.ProgressBar
  $bar.Maximum = 1000; $bar.Height = 12; $bar.Anchor = 'Left, Right'; $bar.Margin = Pad 0 4 10 4; $bar.Style = 'Continuous'
  $progText = New-Object Windows.Forms.Label
  $progText.AutoSize = $true; $progText.ForeColor = $C.Muted; $progText.Anchor = 'Right'; $progText.Margin = Pad 0 0 0 0
  $progText.Text = ' '
  $progRow.Controls.Add($bar, 0, 0)
  $progRow.Controls.Add($progText, 1, 0)
  $grid.Controls.Add($progRow, 0, 5)

  $btnRow = New-Object Windows.Forms.TableLayoutPanel
  $btnRow.Dock = 'Fill'; $btnRow.AutoSize = $true; $btnRow.ColumnCount = 3; $btnRow.RowCount = 1; $btnRow.Margin = Pad 0 0 0 0
  [void]$btnRow.ColumnStyles.Add((New-Object Windows.Forms.ColumnStyle([Windows.Forms.SizeType]::Percent, 100)))
  [void]$btnRow.ColumnStyles.Add((New-Object Windows.Forms.ColumnStyle([Windows.Forms.SizeType]::AutoSize)))
  [void]$btnRow.ColumnStyles.Add((New-Object Windows.Forms.ColumnStyle([Windows.Forms.SizeType]::AutoSize)))

  $links = New-Object Windows.Forms.FlowLayoutPanel
  $links.AutoSize = $true; $links.Anchor = 'Left'; $links.Margin = Pad 0 0 0 0; $links.WrapContents = $false
  function New-Link([string]$Text) {
    $l = New-Object Windows.Forms.LinkLabel
    $l.Text = $Text; $l.AutoSize = $true; $l.Margin = Pad 0 0 14 0
    $l.LinkColor = $C.Muted; $l.ActiveLinkColor = $C.Accent; $l.VisitedLinkColor = $C.Muted; $l.LinkBehavior = 'HoverUnderline'
    $l
  }
  $linkData = New-Link 'Zapisy i logi'
  $linkPage = New-Link 'Strona buildu'
  $links.Controls.Add($linkData)
  $links.Controls.Add($linkPage)

  function New-Button([string]$Text, [bool]$Primary) {
    $b = New-Object Windows.Forms.Button
    $b.Text = $Text; $b.AutoSize = $true; $b.FlatStyle = 'Flat'; $b.Cursor = 'Hand'
    $b.Padding = Pad 12 5 12 5; $b.Margin = Pad 10 0 0 0
    if ($Primary) {
      $b.BackColor = $C.Accent; $b.ForeColor = $C.Bg; $b.FlatAppearance.BorderColor = $C.Accent
      $b.Font = New-Object Drawing.Font('Segoe UI Semibold', 10.5)
    } else {
      $b.BackColor = $C.Panel; $b.ForeColor = $C.Text; $b.FlatAppearance.BorderColor = $C.Line
    }
    $b.Enabled = $false
    $b
  }
  $btnClassic = New-Button 'Graj klasycznie' $false
  $btnPlay    = New-Button 'Graj  (ARPG)' $true
  $btnRow.Controls.Add($links, 0, 0)
  $btnRow.Controls.Add($btnClassic, 1, 0)
  $btnRow.Controls.Add($btnPlay, 2, 0)
  $grid.Controls.Add($btnRow, 0, 6)
  $form.AcceptButton = $btnPlay

  # --- praca w tle ---
  $coreText = $Core.ToString()
  function Start-Background([string]$Kind, [string]$Body, $Arg) {
    $Sync.Error = $null; $Sync.Result = $null; $Sync.Done = 0L; $Sync.Total = 0L
    $rs = [runspacefactory]::CreateRunspace([initialsessionstate]::CreateDefault())
    $rs.Open()
    $rs.SessionStateProxy.SetVariable('Sync', $Sync)
    $rs.SessionStateProxy.SetVariable('Cfg', $Cfg)
    $rs.SessionStateProxy.SetVariable('Arg', $Arg)
    $ps = [powershell]::Create()
    $ps.Runspace = $rs
    $code = "`$ErrorActionPreference='Stop'; `$ProgressPreference='SilentlyContinue'`n$coreText`n" +
              "try { $Body } catch { `$Sync.Error = `$_.Exception.Message } finally { `$Sync.Phase = '' }"
    [void]$ps.AddScript($code)
    $St.Job = @{ Kind = $Kind; Ps = $ps; Handle = $ps.BeginInvoke(); Arg = $Arg }
  }

  function Get-Selected {
    $i = $combo.SelectedIndex
    if ($i -lt 0 -or $i -ge $St.Items.Count) { return $null }
    $St.Items[$i]
  }

  function Update-Items([string]$SelectTag) {
    $map = @{}
    foreach ($r in @($St.Remote)) {
      if (-not $r) { continue }
      $map[$r.Number] = [pscustomobject]@{ Number = $r.Number; Tag = $r.Tag; Branch = $r.Branch; Title = $r.Title; Notes = $r.Notes; Page = $r.Page; Remote = $r; Installed = $false }
    }
    foreach ($v in Get-Installed) {
      if ($map.ContainsKey($v.Number)) { $map[$v.Number].Installed = $true }
      else { $map[$v.Number] = [pscustomobject]@{ Number = $v.Number; Tag = $v.Tag; Branch = $v.Branch; Title = $v.Title; Notes = $v.Notes; Page = ''; Remote = $null; Installed = $true } }
    }
    $items = @($map.Values | Sort-Object Number -Descending)
    if ($Branch) { $items = @($items | Where-Object { $_.Branch -eq $Branch }) }
    if (-not $SelectTag) { $cur = Get-Selected; if ($cur) { $SelectTag = $cur.Tag } }
    $St.Items = $items

    $combo.BeginUpdate()
    $combo.Items.Clear()
    for ($i = 0; $i -lt $items.Count; $i++) {
      $it = $items[$i]
      $name = if ($it.Branch) { "#$($it.Number)  ·  $($it.Branch)" } else { "#$($it.Number)" }
      $flags = @()
      if ($i -eq 0) { $flags += 'najnowsza' }
      $flags += $(if ($it.Installed) { 'na dysku' } else { 'do pobrania' })
      [void]$combo.Items.Add("$name   ($($flags -join ', '))")
    }
    $combo.EndUpdate()

    $idx = 0
    for ($i = 0; $i -lt $items.Count; $i++) {
      if ($SelectTag -and $items[$i].Tag -eq $SelectTag) { $idx = $i }
      elseif (-not $SelectTag -and $Build -and $items[$i].Number -eq $Build) { $idx = $i }
    }
    if ($items.Count) { $combo.SelectedIndex = $idx }
  }

  function Show-Selected {
    $it = Get-Selected
    if (-not $it) { $notes.Text = ''; $linkPage.Visible = $false; return }
    $head = "Build #$($it.Number)"
    if ($it.Branch) { $head += "  ·  gałąź $($it.Branch)" }
    if ($it.Notes -match '(?m)^commit:\s*(\S+)') { $head += "  ·  commit $($Matches[1])" }
    $body = Get-NotesText $it.Notes
    if (-not $body) { $body = '(brak opisu)' }
    $notes.Text = (($head + "`n`n" + $body) -replace "`r", '') -replace "`n", "`r`n"
    $linkPage.Visible = [bool]$it.Page
    Update-Buttons
  }

  function Update-Buttons {
    $it  = Get-Selected
    $job = $St.Job
    $can = $false
    if ($it -and $St.Ready) {
      if (-not $job) { $can = $true }
      elseif ($job.Kind -eq 'install' -and $job.Arg.Tag -eq $it.Tag) { $can = $true }
    }
    $btnPlay.Enabled = $can
    $btnClassic.Enabled = $can
    $combo.Enabled = $St.Ready -and -not $job
  }

  function Start-Install($It) {
    Start-Background 'install' 'Install-Build $Arg; Move-OldBuild; Remove-OldVersions $Arg.Tag' $It.Remote
    Update-Buttons
  }

  function Invoke-Play([bool]$ClassicMode) {
    $it = Get-Selected
    if (-not $it) { return }
    $mode = if ($ClassicMode) { 'classic' } else { 'arpg' }
    if ($St.Job) {
      $St.Pending = $mode
      Log 'Gra uruchomi się zaraz po pobraniu.'
      return
    }
    if (-not $it.Installed) {
      $St.Pending = $mode
      Start-Install $it
      return
    }
    if (Test-GameRunning) { Log 'Gra już działa.' 'warn'; return }
    try {
      $run = Get-Installed | Where-Object { $_.Tag -eq $it.Tag } | Select-Object -First 1
      Move-OldBuild
      Remove-OldVersions $run.Tag
      Start-Game $run $ClassicMode
      $form.Close()
    } catch {
      Log "Błąd: $($_.Exception.Message)" 'error'
    }
  }

  function Complete-Job($Job) {
    switch ($Job.Kind) {
      'check' {
        if ($Sync.Error) {
          $St.Remote = @()
          Log 'Brak połączenia z GitHubem – dostępne są tylko wersje z dysku.' 'warn'
        } else {
          $St.Remote = @($Sync.Result)
        }
        $St.Ready = $true
        Update-Items
        if (-not $St.Items.Count) { Log 'Nie ma żadnej wersji gry. Sprawdź internet i uruchom launcher ponownie.' 'error'; break }
        $newest = $St.Items[0]
        $sel = Get-Selected
        if (-not $Build -and -not $newest.Installed -and $newest.Remote) {
          Start-Install $newest
        } elseif (-not $Sync.Error) {
          Log "Gotowe – $($sel.Title)" 'ok'
        }
      }
      'install' {
        if ($Sync.Error) {
          $St.Pending = $null
          Log "Błąd: $($Sync.Error)" 'error'
          Update-Items
        } else {
          Update-Items $Job.Arg.Tag
          if ($St.Pending) {
            $classicMode = ($St.Pending -eq 'classic')
            $St.Pending = $null
            Invoke-Play $classicMode
          } else {
            Log "Gotowe – $($Job.Arg.Title)" 'ok'
          }
        }
      }
    }
    Update-Buttons
  }

  # --- zdarzenia ---
  $combo.Add_SelectedIndexChanged({ try { Show-Selected } catch { } })
  $btnPlay.Add_Click({ Invoke-Play $false })
  $btnClassic.Add_Click({ Invoke-Play $true })
  $linkData.Add_LinkClicked({
    New-Item -ItemType Directory -Force -Path $Cfg.DataDir | Out-Null
    Start-Process explorer.exe -ArgumentList "`"$($Cfg.DataDir)`""
  })
  $linkPage.Add_LinkClicked({ $it = Get-Selected; if ($it -and $it.Page) { Start-Process $it.Page } })

  $timer = New-Object Windows.Forms.Timer
  $timer.Interval = 150
  $timer.Add_Tick({
    try {
      if ($Sync.Status -ne $St.LastStatus) {
        $St.LastStatus = $Sync.Status
        $status.Text = $Sync.Status
        $status.ForeColor = switch ($Sync.Level) { 'ok' { $C.Ok } 'warn' { $C.Warn } 'error' { $C.Err } default { $C.Muted } }
      }
      switch ($Sync.Phase) {
        'download' {
          if ($bar.Style -ne 'Continuous') { $bar.Style = 'Continuous' }
          if ($Sync.Total -gt 0) {
            $bar.Value = [Math]::Min(1000, [int](1000 * $Sync.Done / $Sync.Total))
            $progText.Text = '{0:N0} / {1:N0} MB  ·  {2:N1} MB/s' -f ($Sync.Done / 1MB), ($Sync.Total / 1MB), $Sync.Speed
          }
        }
        'extract' {
          if ($bar.Style -ne 'Marquee') { $bar.Style = 'Marquee'; $bar.MarqueeAnimationSpeed = 30 }
          $progText.Text = 'instalacja...'
        }
        default {
          if ($bar.Style -ne 'Continuous') { $bar.Style = 'Continuous' }
          if (-not $St.Job) { $bar.Value = 0; $progText.Text = ' ' }
          elseif ($St.Job.Kind -eq 'check') { $progText.Text = ' ' }
        }
      }
      $job = $St.Job
      if ($job -and $job.Handle.IsCompleted) {
        try { [void]$job.Ps.EndInvoke($job.Handle) } catch { if (-not $Sync.Error) { $Sync.Error = $_.Exception.Message } }
        try { $job.Ps.Runspace.Dispose(); $job.Ps.Dispose() } catch { }
        $St.Job = $null
        Complete-Job $job
      }
    } catch {
      Log "Błąd: $($_.Exception.Message)" 'error'
    }
  })

  $form.Add_Shown({
    if ($Offline) {
      $St.Ready = $true
      Update-Items
      if ($St.Items.Count) { Log 'Tryb offline – wersje z dysku.' } else { Log 'Tryb offline, a na dysku nie ma żadnej wersji.' 'error' }
      Update-Buttons
    } else {
      Start-Background 'check' '$Sync.Result = @(Get-RemoteBuilds)' $null
    }
    $timer.Start()
  })
  $form.Add_FormClosing({
    $timer.Stop()
    if ($St.Job) { try { [void]$St.Job.Ps.BeginStop($null, $null) } catch { } }
  })

  [void]$form.ShowDialog()
}

# =============================================================================
try {
  if ($Console -or $NoStart) {
    Invoke-Console
  } else {
    Invoke-Gui
    [Environment]::Exit(0)
  }
  exit 0
} catch {
  $msg = $_.Exception.Message
  if ($Console -or $NoStart) {
    Write-Host ''
    Write-Host "BŁĄD: $msg" -ForegroundColor Red
  } else {
    try {
      Add-Type -AssemblyName System.Windows.Forms
      [void][Windows.Forms.MessageBox]::Show("$msg`n`n$($_.ScriptStackTrace)", 'Gothic Diablo – błąd', 'OK', 'Error')
    } catch { }
  }
  exit 1
}
