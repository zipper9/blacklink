@echo off

set VER=24.01
set ARCH=x86
set CMD7Z=c:\Program Files\7-Zip\7z

"%CMD7Z%" -? >NUL
if errorlevel 1 (
 echo 7z.exe not found in path
 exit 1
)

mkdir Plugins
mkdir Plugins\%ARCH%

curl -L -o MediaInfoDLL.tmp https://mediaarea.net/download/binary/libmediainfo0/%VER%/MediaInfo_DLL_%VER%_Windows_i386_WithoutInstaller.7z
if errorlevel 1 (
 echo Unable to download MediaInfo.DLL
 exit 1
)

"%CMD7Z%" x MediaInfoDLL.tmp MediaInfo.DLL -oPlugins\%ARCH% -y
del MediaInfoDLL.tmp

echo MediaInfo.DLL saved to Plugins\%ARCH%
