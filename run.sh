#!/bin/bash
./build.sh
cd build
python3 -m http.server 8000 --cgi
