#
#

import sys
import re
import json

def analyse(filename):
    print("char *interpret_code = ");
    with open(filename) as inpf:
        for line in inpf:
            line = line.strip()
            print("\" " + line + " \"")
    print(";")

if __name__ == "__main__":
    analyse(sys.argv[1])

# --------------- end of file -----------------------------------------
