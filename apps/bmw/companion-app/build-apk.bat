@echo off
REM One-click APK build for BMW Assistant (Android 15 / API 35)
REM Uses JAVA_HOME if set; else tries Android Studio JBR (JDK 17) so AGP 8.x works.
setlocal
cd /d "%~dp0"

REM Prefer Android Studio JBR (Java 17) if JAVA_HOME not set or points to Java 8
if not defined JAVA_HOME (
    if exist "C:\Program Files\Android\Android Studio\jbr\bin\java.exe" (
        set "JAVA_HOME=C:\Program Files\Android\Android Studio\jbr"
        echo Using Android Studio JBR: %JAVA_HOME%
    )
)
if defined JAVA_HOME (
    if exist "%JAVA_HOME%\bin\java.exe" (
        set "PATH=%JAVA_HOME%\bin;%PATH%"
    )
)

if not exist "gradlew.bat" (
    echo Gradle wrapper not found. Run from Android Studio or install Gradle and run: gradle wrapper --gradle-version 8.7
    exit /b 1
)

echo Building release APK (targetSdk 35)...
call gradlew.bat assembleRelease
if errorlevel 1 (
    echo Build failed. Ensure JDK 11+ is used: set JAVA_HOME to JDK 17 path if needed.
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
