@rem if run in the windows script folder, change to parent
@if not exist plugin ( pushd .. ) else pushd .
@REM call deployit.cmd
plink -P 10022 root@localhost -pw "" "/media/internal/shapespin_plugin"
popd

