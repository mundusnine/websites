@echo off
if not exist generated mkdir generated
start /b /wait "" "xcopy" .\data .\generated\data\ /y /s /e /q
set files= 
for %%i in (*.rxw) do ( call set "files=%%files%% %%i" )
..\..\generator\build\generator.exe --author "Jean-Sebastien Nadeau" --icon "./data/logo_16x16.png" --canonical_url "https://foundry.org" --twitter_handle "@foundry2d" --html --html_header header.html --html_footer footer.html %files%