#!/bin/bash
gcc webfuse.c -o webfuse -D_FILE_OFFSET_BITS=64 -lfuse &&
./webfuse ../mnt_web &&
grep webfuse /etc/mtab