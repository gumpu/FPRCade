#
# Turn FORTH source code into a C string that can be included in a C program.
#

import sys
import re
import json

def analyse(filename):
    parts = filename.split(".")
    code_name = parts[0] + "_code"
    print(f"char *{code_name} = ");
    with open(filename) as inpf:
        for line in inpf:
            line = line.strip()
            print("\" " + line + " \"")
    print(";")

if __name__ == "__main__":
    analyse(sys.argv[1])

# --------------- end of file -----------------------------------------
