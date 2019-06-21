#/usr/bin/sh -e

curl -F "private_id=bea8010f82eaa650cd6488b0" -F "file=@$1" https://monadic-lab.org/submit
