#!/bin/sh
readelf -SW $1 > $2
awk 'match($2, /[.]+/) && NF == 10{print $4 "\t" $6 "\t" $2}' $2 > sec.tmp
awk 'match($3, /[.]+/) && NF == 12{print $5 "\t" $7 "\t" $3}' $2 >> sec.tmp
scripts/format_secs
rm -f sec.tmp $2
