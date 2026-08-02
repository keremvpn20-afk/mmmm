@echo off
title GitHub Auto Upload Tool - ytpavlov_mc_ios
echo ==========================================
echo       GITHUB KOD YUKLEME ARACI (v2)
echo ==========================================
echo.

:: Check if git is in path, if not try to auto-add Visual Studio Git path
where git >nul 2>&1
if %errorlevel% neq 0 (
    echo [!] Git varsayilan olarak bulunamadi. Visual Studio Git yolu ekleniyor...
    set "PATH=%PATH%;C:\Program Files\Microsoft Visual Studio\18\Enterprise\Common7\IDE\CommonExtensions\Microsoft\TeamFoundation\Team Explorer\Git\cmd"
)

:: Re-verify if git is found
where git >nul 2>&1
if %errorlevel% neq 0 (
    echo [HATA] Git bulunamadi! Lutfen Git programini bilgisayariniza kurun.
    pause
    exit /b
)

set /p repo="GitHub Repository URL linkini yapistirin ve Enter'a basin: "

echo.
echo [1/5] Git baslatiliyor...
git init

:: Configure temp identity to prevent git commits from failing
git config user.email "kerem@example.com"
git config user.name "Kerem"

echo [2/5] Proje dosyalari ekleniyor...
git add .

echo [3/5] Degisiklikler kaydediliyor...
git commit -m "Initial commit for ytpavlov_mc_ios client"

echo [4/5] Ana dal ayarlaniyor...
git branch -M main

echo [5/5] GitHub baglantisi kuruluyor...
git remote remove origin >nul 2>&1
git remote add origin %repo%

echo.
echo [!] Kodlar GitHub'a yukleniyor (Giris yapmaniz istenebilir)...
git push -u origin main --force

echo.
echo ==========================================
echo ISLEM TAMAMLANDI!
echo Kodlar GitHub'a yuklendi.
echo GitHub sayfanizdaki 'Actions' sekmesinden dylib dosyasini indirebilirsiniz.
echo ==========================================
echo.
pause
