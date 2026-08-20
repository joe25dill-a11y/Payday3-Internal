$ErrorActionPreference = 'Stop'
$pack = 'E:\PD3-MoolahProject\Payday3-Internal\Himi-Inject-Pack'
$dllSrc = 'E:\PD3-MoolahProject\Payday3-Internal\Build\Release\Payday3-Internal.dll'
$zip = Join-Path $pack 'Injector.zip'

New-Item -ItemType Directory -Force -Path $pack | Out-Null

Write-Host 'Downloading official injector...'
Invoke-WebRequest -Uri 'https://github.com/Omega172/Payday3-Internal/releases/download/Injector/Injector.zip' -OutFile $zip -UseBasicParsing
Expand-Archive -Force -Path $zip -DestinationPath $pack
Remove-Item -Force $zip

Write-Host 'Copying your DLL...'
Copy-Item -Force $dllSrc (Join-Path $pack 'Payday3-Internal.dll')

# Also copy Fixed/FF variants as optional
Copy-Item -Force 'E:\PD3-MoolahProject\Payday3-Internal\Build\Release\Payday3-Internal-Fixed.dll' (Join-Path $pack 'OPTIONAL-Payday3-Internal-Fixed.dll') -ErrorAction SilentlyContinue
Copy-Item -Force 'E:\PD3-MoolahProject\Payday3-Internal\Build\Release\Payday3-Internal-FF.dll' (Join-Path $pack 'OPTIONAL-Payday3-Internal-FF.dll') -ErrorAction SilentlyContinue

@'
HIMI — PD3 Internal inject pack
================================

WHAT TO SEND HIM
  This whole folder.

HOW TO USE (Windows)
  1. Close antivirus false-positives for this folder (injectors get flagged a lot).
  2. Start Payday 3 and get into the main menu / lobby.
  3. Run injector.exe AS ADMINISTRATOR.
  4. Select process: PAYDAY3Client-Win64-Shipping.exe
  5. Select DLL: Payday3-Internal.dll (in this folder)
  6. Inject.
  7. In-game: INSERT = menu, END = unload.

IMPORTANT
  - Use the DLL in THIS folder (Sky's build), not a random UC download.
  - If the game crashes on inject: delete Streamline DLSS files under
    PAYDAY3\Engine\Plugins\Runtime\Nvidia\Streamline\Binaries\ThirdParty\Win64
    (same note as the Internal README).
  - Host Num1 FF / SkyCheats Lua is separate (UE4SS). This pack is the .dll only.

OPTIONAL DLLS
  OPTIONAL-Payday3-Internal-Fixed.dll
  OPTIONAL-Payday3-Internal-FF.dll
  Only use if Sky tells you to — default is Payday3-Internal.dll

Injector source: https://github.com/Omega172/Payday3-Internal/releases/tag/Injector
'@ | Set-Content -Encoding UTF8 (Join-Path $pack 'README-FOR-HIMI.txt')

Write-Host ''
Write-Host 'PACK READY:'
Get-ChildItem $pack | Format-Table Name, Length, LastWriteTime -AutoSize
Write-Host ('Path: ' + $pack)
