$ErrorActionPreference = 'Stop'
Set-Location -LiteralPath $PSScriptRoot
New-Item -ItemType Directory -Force -Path 'dist\obs-sketch\data' | Out-Null
Copy-Item -LiteralPath 'LEEME.md' -Destination 'dist\obs-sketch\data\LEEME.txt'
Copy-Item -LiteralPath 'COPYING.txt' -Destination 'dist\obs-sketch\data\COPYING.txt'
(Get-FileHash 'dist\obs-sketch\bin\64bit\obs-sketch.dll' -Algorithm SHA256).Hash.ToLower() | Set-Content -LiteralPath 'build\plugin.sha256' -Encoding ascii
& 'C:\Windows\Microsoft.NET\Framework64\v4.0.30319\csc.exe' /nologo /target:winexe /platform:x64 /optimize+ /out:dist\OBS-Sketch-1.5-Windows-x64-Setup.exe /win32icon:assets\DjEmiMas.ico /reference:System.Windows.Forms.dll /reference:System.Drawing.dll /resource:assets\DjEmiMas.png,author_logo /resource:assets\DjEmiMas.ico,author_icon /resource:dist\obs-sketch\bin\64bit\obs-sketch.dll,plugin /resource:LEEME.md,readme /resource:COPYING.txt,license /resource:build\plugin.sha256,checksum installer\Setup.cs build\LocaleText.cs
if ($LASTEXITCODE -ne 0) { throw 'No se pudo generar el instalador.' }
Compress-Archive -Path 'dist\obs-sketch' -DestinationPath 'dist\OBS-Sketch-1.5-Windows-x64.zip' -Force
$sourceStage = Join-Path $PSScriptRoot 'build\source-package'
foreach ($folder in @('src','plugin','installer','tests','locale','assets')) {
    New-Item -ItemType Directory -Force -Path (Join-Path $sourceStage $folder) | Out-Null
}
foreach ($file in @('assets\DjEmiMas.png','assets\DjEmiMas.ico','assets\DjEmiMas-original.png','src\obs-sketch.c','src\motion.h','src\i18n.h','locale\strings.json','generate_translations.py','tests\locale-test.c','tests\LocaleTest.cs','plugin\obs-sketch.effect','installer\Setup.cs','tests\gpu-test.c','tests\preview.py','tests\reference-preview.py','tests\motion-test.c','tests\motion-preview.py','build.py','package.ps1','BUILD.md','LEEME.md','COPYING.txt')) {
    Copy-Item -LiteralPath $file -Destination (Join-Path $sourceStage $file)
}
Compress-Archive -Path (Join-Path $sourceStage '*') -DestinationPath 'dist\OBS-Sketch-1.5-source.zip' -Force
Get-FileHash 'dist\OBS-Sketch-1.5-Windows-x64-Setup.exe','dist\OBS-Sketch-1.5-Windows-x64.zip','dist\OBS-Sketch-1.5-source.zip' -Algorithm SHA256 | ForEach-Object { $_.Hash.ToLower() + '  ' + (Split-Path $_.Path -Leaf) } | Set-Content -LiteralPath 'dist\SHA256SUMS.txt' -Encoding ascii





