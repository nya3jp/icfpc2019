#!/bin/sh -e

./fetch.rb
./get_ranking.rb >ranking_data.json
gsutil cp ./ranking_data.json gs://sound-type-system/rankings/
