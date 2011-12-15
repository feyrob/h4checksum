#!/bin/zsh
source ./src/project_settings.rc
rm -rf ./build_std/*
rm -rf ./lib/*
rm -rf ./ex/*

#rm ./ex/$PROJECT_NAME



scons

cp ./build_std/executable ./ex/$PROJECT_NAME 
cp ./build_std/lib_shared.so ./lib/lib$PROJECT_NAME.so 
cp ./build_std/lib_static.a ./lib/lib$PROJECT_NAME.a
./ex/$PROJECT_NAME --file ./data/test_1.png
