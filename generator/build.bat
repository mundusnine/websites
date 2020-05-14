@echo off
start "" /WAIT cmd.exe /c "scoop install gcc"
if not exist build mkdir build
pushd build
gcc ../source/generator.c -o generator.exe
popd