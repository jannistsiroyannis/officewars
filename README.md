# Office Wars

A social game of "conquer the galaxy", for friends or colleagues.

Each turn, players give orders to their units, which are then carried out,
all at once, before the next turn begins.

# Dependencies

In order to build from source, you will require a C compiler (Any compiler
should do, but only tested on GCC 10.2.1)

You will also require the Emscripten tool chain, in order to compile the
client web assembly (Only tested using version 2.0.13).

In order to host the game locally, it's also practical (but not necessary)
to have Python (3) installed, as it contains a convenient GCI-compliant web
server.

# Building and running locally

Assumming above dependencies are met the following commands should be
available:
gcc
emcc
python3

If they are: you should be able to run the run.sh script, which will build
the game and start it in the temporary webserver.

To deploy "for production", any CGI-compliant web server should do
(only tested using Apache2).
Said web server should serve the contents of the build directory, which is
created upon execution of build.sh
