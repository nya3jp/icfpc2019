#/usr/bin/sh -xe

rm -rf tmp-submit
mkdir tmp-submit
cd tmp-submit
unzip ../$1

for file in $(ls *.sol); do
    echo "submitting $file..."
    ../../submit.py --problem $(basename $file .sol) --solver $2 --solution $file &
    sleep 0.1
done

wait
