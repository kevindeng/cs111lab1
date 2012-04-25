#!/bin/bash

cat < /etc/passwd | tr a-z A-Z | sort -u > out || echo sort failed!
