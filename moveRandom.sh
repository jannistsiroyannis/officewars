#!/bin/bash
cd build
./cgi-bin/server randommove
./cgi-bin/server tick

#valgrind ./cgi-bin/server tick
#gdb --args ./cgi-bin/server tick
