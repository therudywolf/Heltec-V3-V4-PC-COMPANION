@echo off
REM One-click APK build for BMW Assistant (Android 15 / API 35)
REM Requires JDK 11 or 17 (set JAVA_HOME if you have multiple Java versions)
cd /d "%~dp0"

REM Check Java version (AGP 8.x requires Java 11+)
for /f "tokens=3" %%v in ('java -version 2^>^&1 ^| findstr /i "version"') do set JVER=%%v
echo %JVER% | findstr /r "1[1-9]\." >nul
if errorlevel 1 (
    echo ERROR: Android build requires JDK 11 or 17. Current: %JVER%
    echo Install JDK 11/17 and set JAVA_HOME, e.g. set JAVA_HOME=C:\Program Files\Java\jdk-17
    exit /b 1
)

if not exist "gradlew.bat" (
    echo Gradle wrapper not found. Generating...
    call gradle wrapper --gradle-version 8.4
    if errorlevel 1 (
        echo Install Gradle or run from Android Studio: Build - Build Bundle(s) / APK(s) - Build APK(s)
        exit /b 1
    )
)

echo Building release APK (targetSdk 35)...
call gradlew.bat assembleRelease
if errorlevel 1 (
    echo Build failed.
    exit /b 1
)

set OUTPUT=app\build\outputs\apk\release\app-release.apk
if exist "%OUTPUT%" (
    echo.
    echo APK built: %OUTPUT%
    echo Install: adb install -r "%OUTPUT%"
) else (
    echo APK not found at %OUTPUT%
    exit /b 1
)
exit /b 0
