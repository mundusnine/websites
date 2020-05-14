start "" /WAIT cmd.exe /c "scoop install coreutils"
start "" /WAIT cmd.exe /c "scoop install wget"
wget https://raw.githubusercontent.com/ryanfleury/websites/master/generator/source/generator.c
mkdir source
mv generator.c ./source/generator.c