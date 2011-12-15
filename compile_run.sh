#!/bin/zsh

# this file is not needed to compile and install
#
# to install simply run:
# % scons
# % scons install
#
# if only the libraries are needed:
# % scons
# % scons install-lib
#
# if only the executable is needed:
# % scons
# % scons install-bin
#


source ./src/project_settings.rc
rm -rf ./build_std/*
rm -rf ./lib/*
rm -rf ./ex/*

#rm ./ex/$PROJECT_NAME



scons

cp ./build_std/$PROJECT_NAME ./ex/$PROJECT_NAME 
#cp ./build_std/lib_shared.so ./lib/lib$PROJECT_NAME.so 
#cp ./build_std/lib_static.a ./lib/lib$PROJECT_NAME.a
cp ./build_std/*.so ./lib/lib$PROJECT_NAME.so 
cp ./build_std/*.a ./lib/lib$PROJECT_NAME.a
./ex/$PROJECT_NAME --file ./data/test_1.png
