Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
Set-Location 'E:\PD3-MoolahProject\Payday3-Internal'

git add -A
Write-Host '=== STATUS AFTER ADD ==='
git status --short | Select-Object -First 20
$count = (git status --short | Measure-Object).Count
Write-Host "Total staged/unstaged lines: $count"

$msg = @'
Sync sky-features with latest local work: ESP glow, jump fix, 3rd person, SDK refresh.

Updates feature code, menu/docs, and refreshed Dumper-7 SDK for current game build.
'@

git commit -m $msg
Write-Host '=== PUSH ==='
git push origin sky-features
Write-Host '=== DONE ==='
git log -1 --oneline
git status -sb
