@echo off
setlocal
if exist .git goto Error

git init .
git remote add mu_feature_dfci https://github.com/Microsoft/mu_feature_dfci
git fetch mu_feature_dfci
git checkout mu_feature_dfci/main -- DfciPkg/UnitTests/DfciTests
rmdir /s /q .git
goto Done

:Error
echo Don't run this command in a directory at the root of a repository. (ie with a .git folder)

:Done
endlocal