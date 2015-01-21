#!/bin/sh

# Converts every EPS file in the current directory to PDF and removes the
# EPS file

for i in ./*.eps; do
    echo "Converting $i..."
    [ -e "${i%.eps}.pdf" ] && rm "${i%.eps}.pdf"
    epstopdf "$i" && rm "$i"
done

exit 0
