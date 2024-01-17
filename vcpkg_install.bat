@echo off

REM Check if the vcpkg directory exists
if exist "vcpkg\" (
    REM If it exists, pull the latest changes
    cd vcpkg
    git pull
) else (
    REM If it doesn't exist, clone the vcpkg repository
    git clone https://github.com/microsoft/vcpkg.git
    cd vcpkg
)

REM Run the bootstrap script
bootstrap-vcpkg.bat -disableMetrics

REM Install required packages
vcpkg x-set-installed %*

REM Return to the original directory
cd ..

