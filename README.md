# hi

C++11 utility library.

Something I use for most of my little C++ programs. Queues instead of threads, channels instead of file descriptors.

## Example

```cc
#include <hi/hi.h>
#include <iostream>

int main(int argc, char** argv) {
  hi::channel::connect("tcp:[::1]:1337", [](hi::error err, hi::channel ch) {
    if (err) { std::cerr << err << "\n"; return; }
    ch.read(4096, [=](hi::error e, hi::data data) {
      if (err) {
        std::cout << "read error: " << err << "\n";
      } else if (data == nullptr) { // EOS
        ch.close([=]{ std::cout << "closed\n"; });
      } else {
        std::cout << "read " << data->size()
                  << " '" << std::string(data->bytes(), data->size()) << "'\n";
      }
      return true; // continue reading
    });
    ch.write("Hello", strlen("Hello"), [=](hi::error err) {
      if (err) { std::cerr << "write error: " << err << "\n"; }
    });
  });
  return hi::main_loop();
}
```

```mk
all: program
include $(shell HI_DEBUG="$(DEBUG)" hi/build_config.sh)
objects := $(patsubst %.cc,%.o,$(wildcard *.cc))
program: libhi $(objects)
  $(LD) $(HI_LDFLAGS) -o $@ $(objects)
.cc.o:
  $(CXX) $(HI_COMMON_FLAGS) $(HI_CXXFLAGS) -MMD -c $< -o $@
clean:
  rm -rf $(objects) $(objects:.o=.d) program
-include ${objects:.o=.d}
.PHONY: clean
```

    $ node -e "require('net').createServer(function (socket) {\
      socket.pipe(socket); }).listen(1337, '::1');" &
    $ make && ./program
    read 5 'Hello'

## MIT License

Copyright (c) 2013 Rasmus Andersson <http://rsms.me/>

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
THE SOFTWARE.
