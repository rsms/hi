#include <hi/hi.h>
#include <iostream>

int main(int argc, char** argv) {
  auto on_connect = [](hi::error err, hi::channel ch) {
    if (err) {
      std::cerr << "failed to connect: " << err << "\n";
      return;
    }

    auto name = std::string("<") + ch->endpoint_name() + (ch->tls() == nullptr ? "> " : " tls> ");
    std::cout << name << "connected\n";

    // Start reading
    ch->read(4096, [=](hi::error e, hi::data data) {
      if (e) {
        std::cout << name << "read error: " << e << "\n";
      } else if (data == nullptr) {
        std::cout << name << "read END\n";
        ch->close([=]{ std::cout << name << "closed\n"; });
      } else {
        std::cout << name << "read " << data->size()
                  << " '" << std::string(data->bytes(), data->size()) << "'"
                  << "\n";
      }
      return true; // yes, we want to continue reading
    });

    // Send a HTTP request
    static const char* http_req0 = "" \
      "GET /lol HTTP/1.1\r\n"
      "Host: localhost\r\n"
      "Connection: close\r\n"
      "\r\n";
    ch->write(http_req0, strlen(http_req0), [=](hi::error e) {
      if (e) { std::cerr << name << "write error: " << e << "\n"; }
    });
  };

  // Setup a TLS context that we will use for our encrypted channels
  hi::tls_context tls_ctx;
  tls_ctx->load_ca_cert_file("ca.crt");

  hi::channel::connect("tcp:127.0.0.1:4430", tls_ctx, on_connect);
  hi::channel::connect("tcp:127.0.0.1:8000",          on_connect);
  hi::channel::connect("tcp:[::1]:4430",     tls_ctx, on_connect);
  hi::channel::connect("tcp:[::1]:8000",              on_connect);

  return hi::main_loop();
}
