@rem if run in the windows script folder, change to parent
@if not exist plugin ( pushd .. ) else pushd .

set STAGING_DIR=..\src

call build\buildit.cmd
copy plugin %STAGING_DIR%
echo filemode.755=plugin > %STAGING_DIR%\package.properties

popd

pause