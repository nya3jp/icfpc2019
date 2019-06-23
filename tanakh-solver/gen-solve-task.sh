#!/bin/sh -ex

cargo build --release

rm -rf tmp
mkdir tmp
cd tmp
cp ../target/release/tanakh-solver .
cp ../solve-task .
tar -czf solve-task.tar.gz tanakh-solver solve-task
mv solve-task.tar.gz ..
cd ..
rm -rf tmp
