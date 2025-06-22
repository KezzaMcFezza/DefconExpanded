# How to Build

## Linux, courtesy of bert_the_turtle

Run the autotools with `./bootstrap.sh`

Configure for compilation with `./configure --enable-dedcon`

(Configuring and building into a separate directory also works)

Build with `make`

Don't be surprised that there is also a `dedwinia` binary, that is not functional.

## macOS, Courtesy of bert_the_turtle

Should work the same as Linux. In the latest test, there were compilation errors, should be easy to fix.

## Windows, courtesy of KezzaMcFezza

Download MSYS2 from `https://www.msys2.org/`

Open the MSYS2 utility from the start menu or by searching MYSYS

Once inside the terminal run these commands...

`pacman -Syu`

`pacman -Su`

Then we get onto the good stuff now we need to install the build tools, so run this command inside the MSYS terminal

`pacman -S mingw-w64-i686-toolchain make autoconf automake libtool mingw-w64-i686-zlib git`

They should not return any errors, and now you are ready to navigate to dedcon-trunk or whatever you have called it

`cd /c/path/to/dedcon-trunk`

i have mine directly on the c drive like this `cd /c/dedcon-trunk`

Once inside the dedcon folder run these commands...

`./bootstrap.sh`

And just for peace of mind run this command also to make sure ranlib is found

Not sure what order but i will try

`LIBS="$LIBS -lz -luuid -lwsock32 -lgnurx"`

`autoreconf -fi`

`export PATH="/mingw32/bin:$PATH"` 
`export PATH="/usr/bin:$PATH"`

Then run the configure command

`./configure --enable-dedcon --host=i686-w64-mingw32 CXX=i686-w64-mingw32-g++ CC=i686-w64-mingw32-gcc`

This `./configure` command can be customized if needed it doesnt have to exactly look like this, however we need to use the 32bit compiler which is `i686`

Then run these two commands after the configuration has completed

`ln -s /mingw32/bin/ranlib /mingw32/bin/i686-w64-mingw32-ranlib`
`ln -s /mingw32/bin/windres /mingw32/bin/i686-w64-mingw32-windres`

i spent hours troubleshooting and that one command above is literally the make or break for this working

Then to build dedcon run
`make clean && make`

You should always do make clean everytime you make changes to the source code to prevent compile errors
After following this tutorial you should be able to successfully build a dedcon.exe, and everytime you want to build again like for example closing the terminal
Just open MSYS again and navigate to dedcon trunk or whatever you have called it

Then run the `./configure` command i have provided

And finally do `make clean && make`









