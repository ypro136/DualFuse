#!/usr/bin/env bash
set -x # show cmds
set -e # fail globally

cd $1

sh $2/configure $3
make 
make install
cd ..
