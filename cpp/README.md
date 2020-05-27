# README

This directory contains the C++ version of the scraping example.  It is built
on top of the [boost], [curl], & [lexbor] libraries and managed with the
[cmake] utility.

## Building
Assuming that you have the libraries installed, this should be as easy as
running cmake and make as shown below.

    $ cmake .
    -- The C compiler identification is AppleClang 11.0.0.11000033
    -- The CXX compiler identification is AppleClang 11.0.0.11000033
    -- Check for working C compiler: /Library/Developer/CommandLineTools/usr/bin/cc
    -- Check for working C compiler: /Library/Developer/CommandLineTools/usr/bin/cc - works
    -- Detecting C compiler ABI info
    -- Detecting C compiler ABI info - done
    -- Detecting C compile features
    -- Detecting C compile features - done
    -- Check for working CXX compiler: /Library/Developer/CommandLineTools/usr/bin/c++
    -- Check for working CXX compiler: /Library/Developer/CommandLineTools/usr/bin/c++ - works
    -- Detecting CXX compiler ABI info
    -- Detecting CXX compiler ABI info - done
    -- Detecting CXX compile features
    -- Detecting CXX compile features - done
    -- Found Boost: /.../cmake/Boost-1.73.0/BoostConfig.cmake (found version "1.73.0") found components: log program_options
    -- Found CURL: /usr/lib/libcurl.dylib (found version "7.64.1")
    -- Configuring done
    -- Generating done
    -- Build files have been written to: /.../milkstreet-scraping/cpp

    $ make
    Scanning dependencies of target scrape
    [ 50%] Building CXX object CMakeFiles/scrape.dir/scrape.cpp.o
    [100%] Linking CXX executable scrape
    [100%] Built target scrape

    $ ./scrape -h
    Usage: ./scrape [options] URL OUTPUT-FILE
    Options:
      -h [ --help ]         produce help message
      -v [ --verbose ]      enable diagnostic output

    $ ./scrape https://www.177milkstreet.com/recipes/maple-whiskey-pudding-cakes output.html
    [2020-05-26 08:07:23.288961] [0x0000000114f43dc0] [info]    retrieving https://www.177milkstreet.com/recipes/maple-whiskey-pudding-cakes
    [2020-05-26 08:07:23.398485] [0x0000000114f43dc0] [info]    retrieved 150237 bytes from https://www.177milkstreet.com/recipes/maple-whiskey-pudding-cakes
    [2020-05-26 08:07:23.402626] [0x0000000114f43dc0] [info]    processed input in 36 chunks
    [2020-05-26 08:07:23.402912] [0x0000000114f43dc0] [info]    writing output to output.html

[boost]: https://boost.org/
[cmake]: https://cmake.org/
[curl]: https://curl.haxx.se/libcurl/
[lexbor]: https://lexbor.com/
