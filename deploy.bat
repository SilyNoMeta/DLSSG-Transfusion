@echo off
setlocal enabledelayedexpansion

:: DLSSG-Transfusion Smart Deploy Script
:: Safely replaces ONLY DLSSG-Transfusion DLL/ASI files without touching OptiScaler or other mods.

set "DIST_DIR=%~dp0build\dist"
set "SRC_DLL=%DIST_DIR%\DLSSG-Transfusion.dll"

if not exist "%SRC_DLL%" (
    echo [ERROR] Could not find built DLL at:
    echo "%SRC_DLL%"
    echo Please build the project first.
    pause
    exit /b 1
)

set "TARGET_DIR=%~1"
if "%TARGET_DIR%"=="" (
    set /p "TARGET_DIR=Enter game folder path (or drag and drop folder here): "
)

:: Strip PowerShell call operator & and surrounding single/double quotes if pasted from terminal
if "!TARGET_DIR:~0,2!"=="& " set "TARGET_DIR=!TARGET_DIR:~2!"
set "TARGET_DIR=!TARGET_DIR:'=!"
set "TARGET_DIR=!TARGET_DIR:"=!"
:: Strip any trailing whitespace
for /f "tokens=* delims= " %%A in ("!TARGET_DIR!") do set "TARGET_DIR=%%A"

if not exist "%TARGET_DIR%" (
    echo [ERROR] Target directory does not exist: "%TARGET_DIR%"
    pause
    exit /b 1
)

echo.
echo Target folder: "%TARGET_DIR%"
echo Scanning for existing DLSSG-Transfusion files (ignoring OptiScaler/other mods)...

set "DEPLOYED=0"
set "PROXIES=dxgi.dll dinput8.dll winmm.dll version.dll DLSSG-Transfusion.dll DLSSG-Transfusion.asi"

for %%F in (%PROXIES%) do (
    if exist "%TARGET_DIR%\%%F" (
        set "IS_TRANSFUSION=0"
        for /f "usebackq delims=" %%I in (`powershell -NoProfile -Command "(Get-Item '%TARGET_DIR%\%%F').VersionInfo.ProductName" 2^>nul`) do (
            if "%%I"=="DLSSG-Transfusion" set "IS_TRANSFUSION=1"
        )
        if "!IS_TRANSFUSION!"=="1" (
            echo  [MATCH] Found Transfusion as: %%F ^(--^> Updating^)
            copy /y "%SRC_DLL%" "%TARGET_DIR%\%%F" >nul
            set /a DEPLOYED+=1
        ) else (
            echo  [SKIP]  Found %%F but it is NOT Transfusion ^(e.g. OptiScaler/system^)
        )
    )
)

if %DEPLOYED%==0 (
    echo.
    echo No existing DLSSG-Transfusion installation detected in this folder.
    echo Choose which proxy name to install as:
    echo  [1] dxgi.dll
    echo  [2] dinput8.dll
    echo  [3] winmm.dll
    echo  [4] version.dll
    echo  [5] DLSSG-Transfusion.asi
    echo  [6] DLSSG-Transfusion.dll
    set /p "CHOICE=Choice (1-6, default 1): "
    if "!CHOICE!"=="" set "CHOICE=1"
    if "!CHOICE!"=="1" set "NEWNAME=dxgi.dll"
    if "!CHOICE!"=="2" set "NEWNAME=dinput8.dll"
    if "!CHOICE!"=="3" set "NEWNAME=winmm.dll"
    if "!CHOICE!"=="4" set "NEWNAME=version.dll"
    if "!CHOICE!"=="5" set "NEWNAME=DLSSG-Transfusion.asi"
    if "!CHOICE!"=="6" set "NEWNAME=DLSSG-Transfusion.dll"

    if exist "%TARGET_DIR%\!NEWNAME!" (
        echo [WARNING] !NEWNAME! already exists and belongs to another mod!
        set /p "CONFIRM=Overwrite anyway? (y/N): "
        if /i not "!CONFIRM!"=="y" (
            echo Cancelled.
            pause
            exit /b 0
        )
    )

    copy /y "%SRC_DLL%" "%TARGET_DIR%\!NEWNAME!" >nul
    echo  - Installed as: !NEWNAME!
    set DEPLOYED=1
)

:: Update config if present
if exist "%DIST_DIR%\DLSSG-Transfusion.json" (
    if not exist "%TARGET_DIR%\DLSSG-Transfusion.json" (
        copy /y "%DIST_DIR%\DLSSG-Transfusion.json" "%TARGET_DIR%\" >nul
        echo  - Copied DLSSG-Transfusion.json
    )
)

echo.
echo [SUCCESS] Done. Updated %DEPLOYED% Transfusion file(s).
timeout /t 3 >nul
