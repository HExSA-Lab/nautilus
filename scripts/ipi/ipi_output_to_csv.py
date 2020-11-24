#!/usr/bin/python3
import re
import sys

with open(sys.argv[1], "r") as f:
    print("# trial,sc,dc,latency")
    for line in f.readlines():
        if re.match('SC: \d+.*', line) is None:
            continue
        if re.match('^#', line) is not None:
                continue
        m = re.search("SC: (\d+) TC: (\d+) TRIAL: (\d+) - (\d+) cycles", line.strip())
        print(m.group(3) + "," + m.group(1) + "," + m.group(2) + "," + m.group(4))


