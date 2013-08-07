# HTTPS client

A very simple HTTP and HTTPS client using `hi::channel`.

You currently need [Nodejs](nodejs.org) for this example to work (for the server). Now start the simple echo server:

    $ node server.js
    listening (http, 127.0.0.1, 8000)
    listening (https, 127.0.0.1, 4430)
    listening (http, ::1, 8000)
    listening (https, ::1, 4430)

Then in another terminal:

    $ make && ./https-client
    <[::1]:8000> connected
    <127.0.0.1:8000> connected
    <[::1]:8000> read 133
      HTTP/1.1 200 OK
      Content-Type: text/plain
      Content-length: 12
      Date: Wed, 07 Aug 2013 02:12:27 GMT
      Connection: close

      Hello World
    <[::1]:8000> read END
    <[::1]:8000> closed
    <127.0.0.1:8000> read 133
      HTTP/1.1 200 OK
      ...
    <127.0.0.1:8000> read END
    <127.0.0.1:8000> closed
    <127.0.0.1:4430 tls> connected
    <[::1]:4430 tls> connected
    <127.0.0.1:4430 tls> read 133
      HTTP/1.1 200 OK
      ...
    <127.0.0.1:4430 tls> read END
    <127.0.0.1:4430 tls> closed
    <[::1]:4430 tls> read 133
      HTTP/1.1 200 OK
      ...
    <[::1]:4430 tls> read END
    <[::1]:4430 tls> closed
