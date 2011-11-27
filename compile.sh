#!/bin/zsh
scons
# TODO
# proper escaping
#
source ./src/project_settings.rc
cp ./build_std/compiler_output_ex ./ex/$PROJECT_NAME

