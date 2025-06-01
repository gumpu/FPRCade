#!/bin/bash

mkdir -p /tmp/dev/
socat -d pty,raw,echo=0,link=/tmp/dev/fprcade pty,raw,echo=0,link=/tmp/dev/terminal

