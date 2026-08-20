$ErrorActionPreference = 'Continue'
$out = 'E:\PD3-MoolahProject\Payday3-Internal\Himi-Inject-Pack'
New-Item -ItemType Directory -Force -Path $out | Out-Null

Write-Host '=== Injector release ==='
try {
  $rel = Invoke-RestMethod -Uri 'https://api.github.com/repos/Omega172/Payday3-Internal/releases/tags/Injector' -Headers @{ 'User-Agent' = 'Cursor' }
  foreach ($a in $rel.assets) {
    Write-Host ($a.name + ' | ' + $a.browser_download_url)
  }
} catch {
  Write-Host ('Injector tag fail: ' + $_.Exception.Message)
}

Write-Host '=== Latest DLL release ==='
try {
  $latest = Invoke-RestMethod -Uri 'https://api.github.com/repos/Omega172/Payday3-Internal/releases/latest' -Headers @{ 'User-Agent' = 'Cursor' }
  Write-Host ('LATEST: ' + $latest.tag_name)
  foreach ($a in $latest.assets) {
    Write-Host ($a.name + ' | ' + $a.browser_download_url)
  }
} catch {
  Write-Host ('Latest fail: ' + $_.Exception.Message)
}

Write-Host '=== Local dlls ==='
Get-ChildItem 'E:\PD3-MoolahProject\Payday3-Internal' -Recurse -Filter '*.dll' -ErrorAction SilentlyContinue |
  Where-Object { $_.FullName -notmatch 'Dumper-7|Intermediate|\.vs' } |
  Select-Object -First 20 FullName, Length, LastWriteTime |
  Format-Table -AutoSize

Get-ChildItem 'E:\PD3-MoolahProject\Build' -Recurse -Filter '*.dll' -ErrorAction SilentlyContinue |
  Select-Object -First 20 FullName, Length, LastWriteTime |
  Format-Table -AutoSize
