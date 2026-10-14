<#
  gothic-diablo – lokalny build (Windows, MSYS2 MinGW64)

  Każda zmiana dalej trafia na GitHuba (CI i launcher działają jak dotąd). Ten skrypt daje
  szybką pętlę na własnym PC: pobiera najnowszy commit, buduje przyrostowo i uruchamia grę
  z tymi samymi zapisami i Gothic.ini co launcher (gra\dane).

  Układ (w folderze launchera):
    dev.ps1, graj_lokalnie.bat, buduj_w_tle.bat   kopie z src\dev (aktualizują się same)
    src\                                          klon repo, build w src\build-dev
    gra\lokalne\<commit>\                         zbudowana gra (2 ostatnie)
    gra\lokalne\build.log, setup.log              logi
    .ccache\                                      cache kompilatora

  Opcje:
    -Setup            jednorazowo: MSYS2 + kompilator, klon repo, pierwszy build
    -Watch            pętla: nowy commit na GitHubie -> build (bez uruchamiania gry)
    -Branch <nazwa>   konkretna gałąź (domyślnie gałąź z najnowszym commitem, jak launcher)
    -NoPull           buduje to, co jest w src (bez pobierania)
    -NoStart          bez uruchamiania gry
    -Classic          tryb klasyczny (bez ARPG)
    -GothicPath ...   folder Gothic II
    -Msys ...         folder MSYS2 (domyślnie C:\msys64, jeśli już jest, inaczej <dysk launchera>:\msys64)
    pozostałe parametry trafiają do Gothic2Notr.exe
#>
[CmdletBinding()]
param(
  [switch]$Setup,
  [switch]$Watch,
  [string]$Branch = '',
  [switch]$NoPull,
  [switch]$NoStart,
  [switch]$Classic,
  [string]$GothicPath = 'F:\SteamLibrary\steamapps\common\Gothic II',
  [string]$Msys = '',
  [string]$Repo = 'https://github.com/wr0b3l/gothic-diablo.git',
  [ValidateRange(5, 600)][int]$Interval = 15,
  [Parameter(ValueFromRemainingArguments = $true)][string[]]$GameArgs
)

$ErrorActionPreference = 'Stop'
$ProgressPreference    = 'SilentlyContinue'
[Console]::OutputEncoding = New-Object Text.UTF8Encoding($false)
[Net.ServicePointManager]::SecurityProtocol = [Net.ServicePointManager]::SecurityProtocol -bor [Net.SecurityProtocolType]::Tls12

# skrypt leży w folderze launchera albo w src\dev
$Root = $PSScriptRoot
if ((Split-Path -Leaf $PSScriptRoot) -eq 'dev' -and (Test-Path (Join-Path $PSScriptRoot '..\.git'))) {
  $Root = Split-Path (Split-Path $PSScriptRoot)
}
$Src   = Join-Path $Root 'src'
$Bld   = Join-Path $Src 'build-dev'
$Out   = Join-Path $Root 'gra\lokalne'
$Data  = Join-Path $Root 'gra\dane'
$Exe   = 'Gothic2Notr.exe'
if (-not $Msys) {
  $Msys = if (Test-Path 'C:\msys64\usr\bin\bash.exe') { 'C:\msys64' } else { Join-Path (Split-Path -Qualifier $Root) 'msys64' }
}
$Mingw = Join-Path $Msys 'mingw64\bin'
$GameArgs = @($GameArgs | Where-Object { $_ })
$script:LogWriter = $null
$script:GitExe = $null

# -----------------------------------------------------------------------------
function Log([string]$Msg, [string]$Level = 'info') {
  $color = switch ($Level) { 'warn' { 'Yellow' } 'ok' { 'Green' } 'error' { 'Red' } default { 'Gray' } }
  Write-Host $Msg -ForegroundColor $color
  if ($script:LogWriter) { $script:LogWriter.WriteLine("[$(Get-Date -Format 'HH:mm:ss')] $Msg") }
}

function Open-Log([string]$Name, [bool]$Append = $false) {
  Close-Log
  New-Item -ItemType Directory -Force -Path $Out | Out-Null
  $script:LogWriter = New-Object IO.StreamWriter((Join-Path $Out $Name), $Append, (New-Object Text.UTF8Encoding($false)))
  $script:LogWriter.AutoFlush = $true
}

function Close-Log {
  if ($script:LogWriter) { $script:LogWriter.Dispose(); $script:LogWriter = $null }
}

# Uruchamia program, pokazuje i loguje wyjście (stdout + stderr), zwraca kod wyjścia.
function Invoke-Logged([string]$File, [string[]]$ArgList) {
  $ErrorActionPreference = 'Continue'
  & $File @ArgList 2>&1 | ForEach-Object {
    $line = "$_"
    Write-Host $line
    if ($script:LogWriter) { $script:LogWriter.WriteLine($line) }
  }
  $LASTEXITCODE
}

function Find-Git {
  $cmd = Get-Command git.exe -ErrorAction SilentlyContinue
  if ($cmd) { return $cmd.Source }
  $candidates = @(
    "$env:ProgramFiles\Git\cmd\git.exe",
    "$env:LOCALAPPDATA\Programs\Git\cmd\git.exe"
  )
  foreach ($p in $candidates) { if (Test-Path $p) { return $p } }
  foreach ($pattern in 'cmd\git.exe', 'bin\git.exe', 'mingw64\bin\git.exe') {
    $fork = Get-ChildItem (Join-Path "$env:LOCALAPPDATA\Fork\gitInstance\*" $pattern) -ErrorAction SilentlyContinue |
      Sort-Object FullName -Descending | Select-Object -First 1
    if ($fork) { return $fork.FullName }
  }
  $msysGit = Join-Path $Msys 'usr\bin\git.exe'
  if (Test-Path $msysGit) { return $msysGit }
  $null
}

function Use-Git {
  if ($script:GitExe) { return }
  $script:GitExe = Find-Git
  if (-not $script:GitExe) { throw 'Nie znaleziono gita – uruchom dev_setup.bat.' }
}

# git w src; ścieżki z ukośnikami działają i w Git for Windows, i w gicie z MSYS2
function Invoke-Git {
  $code = Invoke-Logged $script:GitExe (@('-C', ($Src -replace '\\', '/')) + $args)
  if ($code -ne 0) { throw "git $($args -join ' ') – błąd ($code)" }
}

function Get-Git {
  $ErrorActionPreference = 'Continue'
  $o = & $script:GitExe -C ($Src -replace '\\', '/') @args 2>$null
  if ($LASTEXITCODE -ne 0) { throw "git $($args -join ' ') – błąd ($LASTEXITCODE)" }
  $o
}

function Use-Toolchain {
  if (-not (Test-Path (Join-Path $Mingw 'g++.exe'))) { throw "Brak kompilatora w $Mingw – uruchom dev_setup.bat." }
  if (($env:PATH -split ';') -notcontains $Mingw) { $env:PATH = "$Mingw;$env:PATH" }
  $env:CCACHE_DIR = Join-Path $Root '.ccache'
}

function Test-GameRunning { [bool](Get-Process -Name 'Gothic2Notr' -ErrorAction SilentlyContinue) }

# Jeden build naraz (graj_lokalnie.bat i buduj_w_tle.bat dzielą src i build-dev).
function Enter-BuildLock {
  New-Item -ItemType Directory -Force -Path $Out | Out-Null
  $path = Join-Path $Out '.lock'
  $waited = $false
  while ($true) {
    try { return [IO.File]::Open($path, 'OpenOrCreate', 'ReadWrite', 'None') }
    catch {
      if (-not $waited) { Log 'Trwa build w drugim oknie – czekam...' 'warn'; $waited = $true }
      Start-Sleep -Seconds 1
    }
  }
}

# Kopiuje dev.ps1 i .bat z src\dev do folderu launchera, jeśli się zmieniły.
function Install-DevFiles {
  $from = Join-Path $Src 'dev'
  if (-not (Test-Path $from) -or ($Root -eq $from)) { return }
  foreach ($f in Get-ChildItem $from -File | Where-Object { $_.Extension -in '.ps1', '.bat' }) {
    $dst = Join-Path $Root $f.Name
    if ((Test-Path $dst) -and ((Get-FileHash $dst).Hash -eq (Get-FileHash $f.FullName).Hash)) { continue }
    Copy-Item $f.FullName $dst -Force
    Log "Zaktualizowano $($f.Name)."
  }
}

# -----------------------------------------------------------------------------
# Pobiera najnowszy commit. Zwraca $true, jeśli kod w src się zmienił.
function Update-Source {
  $before = Get-Git rev-parse HEAD
  Invoke-Git fetch --prune --quiet origin
  $br = $Branch
  if (-not $br) {
    $br = Get-Git for-each-ref --sort=-committerdate --format='%(refname:lstrip=3)' refs/remotes/origin |
      Where-Object { $_ -and $_ -ne 'HEAD' } | Select-Object -First 1
  }
  if (-not $br) { throw 'Na GitHubie nie ma żadnej gałęzi.' }
  $remote = Get-Git rev-parse "origin/$br"
  $current = Get-Git rev-parse --abbrev-ref HEAD
  if ($before -eq $remote -and $current -eq $br) { return $false }

  if (Get-Git status --porcelain --untracked-files=no) {
    Log "W src są lokalne zmiany – nie pobieram $br, buduję to, co jest." 'warn'
    return $false
  }
  Log "Pobieram $br ($($remote.Substring(0, 7)))..."
  Invoke-Git checkout -q -B $br "origin/$br"
  Invoke-Git submodule update --init --recursive --depth 1
  Install-DevFiles
  $true
}

function Get-CommitInfo {
  $ErrorActionPreference = 'Continue'
  $sha = Get-Git rev-parse --short=7 HEAD
  $dirty = [bool](Get-Git status --porcelain --untracked-files=no)
  [pscustomobject]@{
    Sha     = $sha
    Branch  = Get-Git rev-parse --abbrev-ref HEAD
    Subject = Get-Git log -1 --format=%s
    Dirty   = $dirty
    Tag     = if ($dirty) { "$sha-zmiany" } else { $sha }
  }
}

function Invoke-Configure([bool]$Lld) {
  $flags = '-O2 -g1 -DNDEBUG'   # jak RelWithDebInfo w CI, ale lżejsze info debug (szybsze kompilowanie i linkowanie)
  $ld = if ($Lld) { '-fuse-ld=lld' } else { '' }
  $a = @('-S', $Src, '-B', $Bld, '-G', 'Ninja',
         '-DCMAKE_BUILD_TYPE=RelWithDebInfo',
         "-DCMAKE_C_FLAGS_RELWITHDEBINFO=$flags",
         "-DCMAKE_CXX_FLAGS_RELWITHDEBINFO=$flags",
         "-DCMAKE_EXE_LINKER_FLAGS=$ld",
         "-DCMAKE_SHARED_LINKER_FLAGS=$ld")
  if (Test-Path (Join-Path $Mingw 'ccache.exe')) {
    $a += '-DCMAKE_C_COMPILER_LAUNCHER=ccache', '-DCMAKE_CXX_COMPILER_LAUNCHER=ccache'
  }
  Log "Konfiguracja CMake (lld: $Lld)..."
  if ((Invoke-Logged (Join-Path $Mingw 'cmake.exe') $a) -ne 0) { throw 'Konfiguracja CMake nie powiodła się – szczegóły w gra\lokalne\build.log.' }
}

# Buduje grę. Zwraca opis commita.
function Invoke-Build([bool]$AllowLldFallback = $false) {
  Use-Toolchain
  $info = Get-CommitInfo
  $header = "#pragma once`nstatic const char* appBuild = `"gothic-diablo lokalny $($info.Branch) $($info.Tag)`";`n"
  $headerFile = Join-Path $Src 'game\build.h'
  if (-not (Test-Path $headerFile) -or ([IO.File]::ReadAllText($headerFile) -ne $header)) {
    [IO.File]::WriteAllText($headerFile, $header)
  }
  if (-not (Test-Path (Join-Path $Bld 'build.ninja'))) { Invoke-Configure $true }

  Log "Buduję $($info.Branch) $($info.Tag): $($info.Subject)"
  $sw = [Diagnostics.Stopwatch]::StartNew()
  $cmake = Join-Path $Mingw 'cmake.exe'
  $code = Invoke-Logged $cmake @('--build', $Bld, '--target', 'Gothic2Notr')
  if ($code -ne 0 -and $AllowLldFallback) {
    Log 'Build z lld się nie udał – próbuję zwykłym linkerem.' 'warn'
    Invoke-Configure $false
    $code = Invoke-Logged $cmake @('--build', $Bld, '--target', 'Gothic2Notr')
  }
  if ($code -ne 0) { throw "Build nie powiódł się – szczegóły w gra\lokalne\build.log." }
  Log ("Zbudowano w {0:N0} s." -f $sw.Elapsed.TotalSeconds) 'ok'
  $info
}

# Kopiuje grę do gra\lokalne\<commit> (gra uruchomiona z kopii nie blokuje kolejnego builda).
function Publish-Local($Info) {
  $bin = Join-Path $Bld 'opengothic'
  $dir = Join-Path $Out $Info.Tag
  $builtExe = Join-Path $bin $Exe
  $copyExe  = Join-Path $dir $Exe
  $same = (Test-Path $copyExe) -and ((Get-Item $copyExe).LastWriteTimeUtc -eq (Get-Item $builtExe).LastWriteTimeUtc) -and
          ((Get-Item $copyExe).Length -eq (Get-Item $builtExe).Length)
  if (-not $same) {
    New-Item -ItemType Directory -Force -Path $dir | Out-Null
    try {
      Copy-Item $builtExe $dir -Force
      Get-ChildItem $bin -Filter '*.dll' | Copy-Item -Destination $dir -Force
      foreach ($dll in 'libgcc_s_seh-1.dll', 'libstdc++-6.dll', 'libwinpthread-1.dll') {
        Copy-Item (Join-Path $Mingw $dll) $dir -Force
      }
    } catch {
      throw "Nie mogę podmienić $dir (gra z tej wersji jest uruchomiona?): $($_.Exception.Message)"
    }
    "$($Info.Branch) $($Info.Tag)`n$($Info.Subject)`n" | Set-Content (Join-Path $dir 'commit.txt') -Encoding UTF8
  }
  (Get-Item $dir).LastWriteTime = Get-Date

  # zostaw 2 najnowsze + uruchomioną
  $inUse = @(Get-Process -Name 'Gothic2Notr' -ErrorAction SilentlyContinue | ForEach-Object { try { Split-Path $_.Path } catch { } })
  Get-ChildItem $Out -Directory | Where-Object { $_.Name -match '^[0-9a-f]{7}' } |
    Sort-Object LastWriteTime -Descending | Select-Object -Skip 2 |
    Where-Object { $inUse -notcontains $_.FullName } |
    ForEach-Object { try { Remove-Item $_.FullName -Recurse -Force } catch { } }
  $dir
}

function Start-Game([string]$Dir) {
  if (-not (Test-Path (Join-Path $GothicPath 'Data'))) {
    throw "Nie znaleziono Gothic II w '$GothicPath'. Podaj -GothicPath."
  }
  if (Test-GameRunning) { Log 'Gra już działa – zamknij ją i uruchom ponownie.' 'warn'; return }
  New-Item -ItemType Directory -Force -Path $Data | Out-Null
  $argList = @('-g', "`"$GothicPath`"")
  if ($Classic) { $argList += '-classic' }
  if ($GameArgs) { $argList += $GameArgs }
  Log "Uruchamiam $(Split-Path -Leaf $Dir)..."
  Start-Process -FilePath (Join-Path $Dir $Exe) -ArgumentList $argList -WorkingDirectory $Data
}

# -----------------------------------------------------------------------------
function Invoke-Msys([string]$Cmd, [bool]$MustSucceed = $true) {
  $env:MSYSTEM = 'MSYS'
  $env:CHERE_INVOKING = '1'
  $code = Invoke-Logged (Join-Path $Msys 'usr\bin\bash.exe') @('-lc', $Cmd)
  if ($MustSucceed -and $code -ne 0) { throw "MSYS2: '$Cmd' – błąd ($code)" }
}

function Stop-MsysProcesses {
  Get-Process -ErrorAction SilentlyContinue |
    Where-Object { $_.Path -and $_.Path.StartsWith($Msys, [StringComparison]::OrdinalIgnoreCase) } |
    ForEach-Object { try { $_.Kill() } catch { } }
}

function Invoke-Setup {
  Open-Log 'setup.log'
  Log "Folder launchera: $Root"
  Log "MSYS2: $Msys"

  # 1. MSYS2
  if (-not (Test-Path (Join-Path $Msys 'usr\bin\bash.exe'))) {
    if ((Split-Path -Leaf $Msys) -ne 'msys64') { throw "-Msys musi kończyć się na \msys64 (np. G:\msys64)." }
    $sfx = Join-Path $env:TEMP 'msys2-base-x86_64-latest.sfx.exe'
    Log 'Pobieram MSYS2 (~100 MB)...'
    Invoke-WebRequest 'https://github.com/msys2/msys2-installer/releases/latest/download/msys2-base-x86_64-latest.sfx.exe' -OutFile $sfx -UseBasicParsing
    Log 'Rozpakowuję MSYS2...'
    & $sfx -y "-o$(Split-Path $Msys)" | Out-Null
    if ($LASTEXITCODE -ne 0 -or -not (Test-Path (Join-Path $Msys 'usr\bin\bash.exe'))) { throw 'Rozpakowanie MSYS2 nie powiodło się.' }
    Remove-Item $sfx -Force
  }
  Log 'Aktualizuję MSYS2 (pierwszy raz kilka minut)...'
  Invoke-Msys 'true'
  Invoke-Msys 'pacman -Syuu --noconfirm' $false
  Stop-MsysProcesses
  Invoke-Msys 'pacman -Syuu --noconfirm'

  $pkgs = @('gcc', 'cmake', 'ninja', 'glslang', 'vulkan-headers', 'vulkan-loader', 'ccache', 'lld') |
    ForEach-Object { "mingw-w64-x86_64-$_" }
  if (-not (Find-Git)) { $pkgs += 'git' }
  Log 'Instaluję kompilator i narzędzia...'
  Invoke-Msys "pacman -S --needed --noconfirm $($pkgs -join ' ')"

  # 2. klon repo
  Use-Git
  Log "git: $($script:GitExe)"
  if (-not (Test-Path (Join-Path $Src '.git'))) {
    Log "Klonuję $Repo do $Src..."
    $code = Invoke-Logged $script:GitExe @('-c', 'core.longpaths=true', 'clone', '--filter=blob:none', $Repo, ($Src -replace '\\', '/'))
    if ($code -ne 0) { throw 'git clone nie powiódł się.' }
    Invoke-Git config core.longpaths true
  }
  Invoke-Git submodule update --init --recursive --depth 1

  # 3. pierwszy build
  $lock = Enter-BuildLock
  try {
    [void](Update-Source)
    Install-DevFiles
    Open-Log 'build.log'
    $info = Invoke-Build $true
    $dir = Publish-Local $info
  } finally { $lock.Dispose() }

  Open-Log 'setup.log' $true
  Log ''
  Log "Gotowe: $dir" 'ok'
  Log 'Graj:           graj_lokalnie.bat   (pobiera najnowszy commit, buduje, uruchamia)' 'ok'
  Log 'Build w tle:    buduj_w_tle.bat     (zostaw otwarte – buduje każdy nowy commit od razu)' 'ok'
  Log 'Launcher działa jak dotąd (buildy z GitHuba).' 'ok'
}

function Invoke-Once {
  Use-Git
  if (-not (Test-Path (Join-Path $Src '.git'))) { throw "Brak $Src – uruchom najpierw dev_setup.bat." }
  $lock = Enter-BuildLock
  try {
    Open-Log 'build.log'
    if (-not $NoPull) { [void](Update-Source) }
    $info = Invoke-Build
    $dir = Publish-Local $info
  } finally { $lock.Dispose() }
  if (-not $NoStart) { Start-Game $dir }
}

function Invoke-Watch {
  Use-Git
  if (-not (Test-Path (Join-Path $Src '.git'))) { throw "Brak $Src – uruchom najpierw dev_setup.bat." }
  Log "Czekam na nowe commity (co $Interval s). Zamknij okno, żeby przerwać." 'ok'
  $last = ''
  while ($true) {
    $lock = $null
    try {
      $lock = Enter-BuildLock
      if (-not $NoPull) { [void](Update-Source) }
      $head = Get-Git rev-parse HEAD
      if ($head -ne $last) {
        $last = $head
        Open-Log 'build.log'
        $info = Invoke-Build
        $dir = Publish-Local $info
        Log "Gotowe: $($info.Branch) $($info.Tag) – $($info.Subject)" 'ok'
        Log 'Graj: graj_lokalnie.bat' 'ok'
        try { [Console]::Beep(880, 150); [Console]::Beep(1175, 200) } catch { }
        Close-Log
      }
    } catch {
      Log "Błąd: $($_.Exception.Message)" 'error'
      try { [Console]::Beep(330, 400) } catch { }
      Close-Log
    } finally {
      if ($lock) { $lock.Dispose() }
    }
    Start-Sleep -Seconds $Interval
  }
}

# -----------------------------------------------------------------------------
try {
  if ($Setup)     { Invoke-Setup }
  elseif ($Watch) { Invoke-Watch }
  else            { Invoke-Once }
  Close-Log
  exit 0
} catch {
  Log ''
  Log "BŁĄD: $($_.Exception.Message)" 'error'
  Close-Log
  exit 1
}
