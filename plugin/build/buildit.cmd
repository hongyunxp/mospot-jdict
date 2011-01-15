@echo off

@rem if run in the windows script folder, change to parent
if not exist src ( pushd .. ) else pushd .

@rem Set the device you want to build for to 1
set PRE=1
set PIXI=1
set DEBUG=0

@rem List your source files here
setlocal EnableDelayedExpansion
set SRC=
for %%c in (src\*.c src\*.cpp) do set SRC=!SRC! %%c
@rem setlocal DisableDelayedExpansion
@rem set SRC=src\plugin.cpp src\dictionary.cpp

@rem List the libraries needed
set LIBS=-lSDL -lGLESv2 -lpdl -lm -Llib -lsqlite3 -lcjson

@rem Name your output executable
set OUTFILE=plugin

set CFLAGS=-std=gnu99 -O2

if %PRE% equ 0 if %PIXI% equ 0 goto :END

if %DEBUG% equ 1 (
   set DEVICEOPTS=-g
) else (
   set DEVICEOPTS=
)

if %PRE% equ 1 (
   set DEVICEOPTS=%DEVICEOPTS% -mcpu=cortex-a8 -mfpu=neon -mfloat-abi=softfp
)

if %PIXI% equ 1 (
   set DEVICEOPTS=%DEVICEOPTS% -mcpu=arm1136jf-s -mfpu=vfp -mfloat-abi=softfp
)

echo %DEVICEOPTS%

arm-none-linux-gnueabi-gcc %DEVICEOPTS% %CFLAGS% -o %OUTFILE% %SRC% "-I%PALMPDK%\include" "-I%PALMPDK%\include\SDL" "-L%PALMPDK%\device\lib" -Wl,--allow-shlib-undefined %LIBS%

popd
goto :EOF

:END
echo Please select the target device by editing the PRE/PIXI variable in this file.
popd
exit /b 1

