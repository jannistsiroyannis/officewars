#!/bin/bash

rm -rf build
mkdir -p build/cgi-bin

# Build server
gcc -lm -O0 -g src/common/*.c src/common/math/*.c src/server/*.c -o build/cgi-bin/server

# Build web client
# Don't use -s ALLOW_MEMORY_GROWTH=1 , it potentially invalidates pointers when it happens (transparent in wasm, but not when exporting pointers to js!
emcc src/client/*.c src/common/*.c src/common/math/*.c -s WASM=1 -s FETCH=1 -o client
mv client.wasm build/
mv client build/client.js

emcc src/lazy-client/*.c src/common/*.c src/common/math/*.c -s WASM=1 -s FETCH=1 -o lazy-client -s EXPORTED_FUNCTIONS='["_loadGameList","_loadGame","_getGameCount","_getGame","_getGameName","_getGameId","_getPlayerCount","_getPlayerName","_getPlayerColor","_getNodeCount","_getNodeConnected","_getNodeControlledBy","_getNodeX","_getNodeY","_getNodeZ","_getTurnCount","_stepGameHistory"]' -s EXPORTED_RUNTIME_METHODS='["ccall","cwrap"]'
cp -r src/client/web/* build/
mv lazy-client.wasm build/
mv lazy-client build/lazy-client.js
