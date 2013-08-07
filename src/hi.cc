#include <hi/hi.h>
#include <uv.h>
#include <queue>

#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
#include <openssl/ssl.h>
#include <openssl/bio.h>
#include <openssl/err.h>

#if !defined(__STDC_NO_THREADS__)
#include <thread>
#include <future>
#endif

// For thread-local data storage
// #ifdef _WIN32
//   // TODO http://msdn.microsoft.com/en-us/library/windows/desktop/ms686991(v=vs.85).aspx
// #else
//   #define _MULTI_THREADED
//   #include <pthread.h>
//   void *pthread_getspecific(pthread_key_t key);
// #endif

namespace hi {


// ------------------------------------------------------------------------------------------------
//                                            async
// ------------------------------------------------------------------------------------------------
#pragma mark - async


void async(fun<void()> f) {
  // TODO: thread pool with an unlocked limit of `unsigned int std::thread::hardware_concurrency()`
  #if !defined(__STDC_NO_THREADS__)
  std::async(std::launch::async, f);
  #else
  #error not implemented
  #endif
}


// ------------------------------------------------------------------------------------------------
//                                            queue
// ------------------------------------------------------------------------------------------------
#pragma mark - queue

static queue_* volatile _main_queue = 0;
static queue _main_queue_ref;


static error loop_error(uv_loop_t* loop) {
  uv_err_t e = uv_last_error(loop);
  return error(std::string(uv_err_name(e)) + ": " + uv_strerror(e), e.code);
}


struct queue_::impl {
  impl(const std::string& l): label(l) {
    loop = uv_loop_new();
    int r = uv_sem_init(&idle_sem, 0);
    assert(r == 0);
  }

  ~impl() {
    std::cout << "queue_::impl::~impl()\n";
    uv_loop_delete(loop);
  }

  void wake_up_from_idle() {
    if (is_idle != 0 && hi_atomic_swap(&is_idle, 0) == 1) {
      printf("[queue_::impl] uv_sem_post(&idle_sem)\n");
      uv_sem_post(&idle_sem);
    }
  }

  int main() {
    thread_id = uv_thread_self();
    printf("ENTER queue_::impl::main T=%lu\n", thread_id);

    stopped = 0;
    while (stopped == 0) {
      // printf("run\n");
      while (uv_run(loop, UV_RUN_ONCE) != 0) {
        if (stopped != 0) {
          goto exit_loop;
        }
        // printf("process more events (stopped=%ld)\n", stopped);
      }
      // No queued events
      printf("idle\n");

      is_idle = 1;
      uv_sem_wait(&idle_sem);
    }

   exit_loop:
    uv_stop(loop);
    printf("EXIT queue_::impl::main T=%lu\n", thread_id);
    delete this;
    return 0;
  }

  void stop() {
    hi_atomic_swap(&stopped, 1);
    wake_up_from_idle();
  }

  std::string   label;
  uv_thread_t   thread;
  unsigned long thread_id = 0;
  uv_loop_t*    loop;
  uv_idle_t     loop_idler;
  uv_sem_t      idle_sem;
  volatile long is_idle = 0;
  volatile long stopped = 1;

  // struct data_pool_ {
  //   struct ent {
  //     const char* bytes;
  //     size_t size;
  //   };
  //   size_t                 sum_size = 0;
  //   std::forward_list<ent> list;
  // } data_pool;
};


queue queue_::create(const std::string& label) {
  return std::make_shared<queue_>(new impl(label));
  // return queue(new queue_(new impl(label)));
}


queue_::~queue_() {
  std::cout << "queue_::~queue_()\n";
  if (_impl->stopped != 0) {
    // Has a thread running. Stop that thread and let impl::main() delete _impl
    // when the thread has been properly stopped.
    _impl->stop();
  } else {
    delete _impl;
  }
}


bool queue_::is_current() const {
  return (_impl->thread_id == uv_thread_self());
}


void queue_::resume() {
  assert(this != _main_queue);
  assert(_impl->thread == 0);
  int status = uv_thread_create(&_impl->thread, [](void* impl) {
    static_cast<queue_::impl*>(impl)->main();
  }, _impl);
  assert(status == 0);
}


void queue_::async(fun<void()> b) {
  uv_async_t* handle = new uv_async_t;
  handle->data = new fun<void()>(b);
  int r = uv_async_init(_impl->loop, handle, [](uv_async_t* handle, int status) {
    fun<void()>* b = (fun<void()>*)handle->data;
    if (b != 0) {
      // Since libuv says this could be called more than once
      (*b)();
      *b = []{}; // causes std::function to release its underlying ref
      delete b;
      handle->data = 0;
      uv_close((uv_handle_t*)handle, [](uv_handle_t* handle) {
        delete handle;
      });
      // uv_unref((uv_handle_t*)handle);
      // delete handle;
    }
  });
  assert(r == 0);
  r = uv_async_send(handle);
  assert(r == 0);
  _impl->wake_up_from_idle();
}


queue main_queue() {
  if (_main_queue == 0) {
    queue_* q = new queue_(new queue_::impl("main"));
    if (!hi_atomic_cas_bool(&_main_queue, 0, q)) {
      // someone else was faster than us
      delete q;
    } else {
      _main_queue_ref.reset(_main_queue);
    }
  }
  return _main_queue_ref;
}

// queue queue::current() {
//   return nullptr;
// }

// void main_exit() {
//   main_queue()->_impl->stop();
// }

int main_loop() {
#if 0 // no auto-exit when empty
  return main_queue()->_impl->main();
#else
  uv_run(main_queue()->_impl->loop, UV_RUN_DEFAULT);
  return 0;
#endif
}

bool main_next() {
  return (uv_run(main_queue()->_impl->loop, UV_RUN_ONCE) != 0);
}

bool main_next_nowait() {
  return (uv_run(main_queue()->_impl->loop, UV_RUN_NOWAIT) != 0);
}


// ------------------------------------------------------------------------------------------------
//                                            semaphore
// ------------------------------------------------------------------------------------------------
#pragma mark - semaphore


struct semaphore::S {
  uv_sem_t sem;
};

semaphore::semaphore(unsigned int value) : semaphore(new S) { uv_sem_init(&self->sem, value); }
void semaphore::dealloc(S* self) { uv_sem_destroy(&self->sem); delete self; }
void semaphore::signal() const { uv_sem_post(&self->sem); }
void semaphore::wait() const { uv_sem_wait(&self->sem); }
// int uv_sem_init(uv_sem_t* sem, unsigned int value);
// void uv_sem_destroy(uv_sem_t* sem);
// void uv_sem_post(uv_sem_t* sem);
// void uv_sem_wait(uv_sem_t* sem);
// int uv_sem_trywait(uv_sem_t* sem);


// ------------------------------------------------------------------------------------------------
//                                             error
// ------------------------------------------------------------------------------------------------
#pragma mark - error


struct error::S : ref_counted {
  int         code;
  std::string msg;
  S(int c, const std::string& m) : code(c), msg(m) {}
  S(int c, std::string&& m) : code(c), msg(m) {}
};

error::error(const std::string& msg, int code) : error(new S(code, msg)) {}
error::error(std::string&& msg, int code) : error(new S(code, msg)) {}
void error::dealloc(S* self) { delete self; }
int error::code() const { return self->code; }
const std::string& error::message() const { return self->msg; }


// ------------------------------------------------------------------------------------------------
//                                           tls_context
// ------------------------------------------------------------------------------------------------
#pragma mark - tls_context


struct tls_context::S : ref_counted {
  SSL_CTX* ssl_handle;
};


tls_context::tls_context() : tls_context(new S) {
  static once_flag o;
  once(o, []{
    SSL_library_init();
    SSL_load_error_strings();
    ERR_load_BIO_strings();
    OpenSSL_add_all_algorithms();
  });

  // self->ssl_handle = SSL_CTX_new(SSLv23_client_method());
  self->ssl_handle = SSL_CTX_new(SSLv23_method());
  if (self->ssl_handle == 0) {
    // TODO: error handling
    ERR_print_errors_fp(stderr);
    abort();
  }
}


void tls_context::dealloc(S* self) {
  // std::cerr << "tls_context::dealloc @ " << (void*)self << "\n";
  SSL_CTX_free(self->ssl_handle);
  delete self;
}


void tls_context::load_ca_cert_file(const char* path) {
  // int r = SSL_CTX_use_PrivateKey(self->ssl_handle, EVP_PKEY *pkey);
  // int SSL_CTX_load_verify_locations(SSL_CTX *ssl_handle, const char *CAfile, const char *CApath);
  int r = SSL_CTX_load_verify_locations(self->ssl_handle, path, NULL);
  if (r == 0) {
    ERR_print_errors_fp(stderr);
    assert(r != 0);
  }
}


// ------------------------------------------------------------------------------------------------
//                                            channel
// ------------------------------------------------------------------------------------------------
#pragma mark - channel

typedef fun<bool(error,data)> channel_read_cb;
typedef fun<void(error,channel)> channel_connect_cb;

struct tls_init_job;

enum class channel_type {
  TCP
};

// channel state
struct channel::S : ref_counted {
  channel_type  _type;
  uv_stream_t*  _stream = 0;
  queue         _q;

  // While active, this holds a reference to the parent channel. This make it possible to guarantee
  // that a channel is not deallocated while reading.
  struct read_context {
    channel         ch; // only for holding a reference
    channel_read_cb cb = 0;
    size_t          max_size = 0;
    bool            reading = false;

    void begin(const channel* c, channel_read_cb f, size_t z) {
      ch = c; cb = f; max_size = (z == 0) ? SIZE_MAX : z; }
    void stop() {
      if (reading) {
        uv_read_stop(ch->self->_stream);
        reading = false;
      }
    }
    void end() {
      std::cerr << "read_context::end @ " << (void*)this << " self=" << (void*)ch->self << "\n";
      if (ch != nullptr) {
        stop();

        // Note: There once was a bug here where the channel was captured by `cb`, and the callsite
        // that called this function (end()) was the `cb` implementation. We capture a local
        // reference to the channel here, so that it's guaranteed to exists at least until this
        // call returns.
        channel chlocal = ch;

        cb = nullptr; assert((bool)cb == false);
        ch = nullptr;
      }
    }
    bool active() { return ch != nullptr; }
    // ~read_context() { std::cerr << "~read_context @ " << (void*)this << "\n"; }
  } _rctx;

  // Holds TLS/SSL state for secure channels
  struct tls_session {
    tls_context ctx;
    SSL* session;
    BIO* in_bio;
    BIO* out_bio;
    tls_init_job* init_job = nullptr;

    tls_session(tls_context c) : ctx(c) {
      // Note: SSL_clear <= "reset SSL object to allow another connection"
      session = SSL_new(ctx->self->ssl_handle);
      in_bio = BIO_new(BIO_s_mem());
      out_bio = BIO_new(BIO_s_mem());
      SSL_set_bio(session, in_bio, out_bio);
    }
    ~tls_session() { SSL_free(session); /* frees the BIOs too */ }
    bool is_initiated() const { return SSL_is_init_finished(session); }
    void init_end(error e = nullptr); // not impl here since need access to init_job struct
  } * tls = nullptr;

  S(channel_type t, queue q) : _type(t), _q(q) {}
  // ~S() { std::cerr << "channel::S::~S @ " << (void*)this << "\n"; }
};

tls_context channel::tls() const {
  return self->tls != nullptr ? tls_context(self->tls->ctx) : nullptr;
}


channel::channel() : self(0) {}


void channel::dealloc(S* self) {
  std::cerr << "channel::dealloc @ " << (void*)self << "\n";
  if (self->_stream != 0) { delete self->_stream; }
  if (self->tls) { delete self->tls; }
  delete self;
}


// channel::channel_type channel::type() const {
//   return self->_type;
// }


std::string channel::endpoint_name() const {
  switch (self->_type) {
    case channel_type::TCP: {
      struct sockaddr sa;
      int namelen = sizeof(sa);
      int r = uv_tcp_getpeername((uv_tcp_t*)self->_stream, &sa, &namelen);
      if (r == 0) {
        if (sa.sa_family == AF_INET) {
          // x.x.x.x:p
          char buf[17];
          auto si = (struct sockaddr_in*)&sa;
          uv_err_t e = uv_inet_ntop(sa.sa_family, (const void*)&si->sin_addr, buf, 17);
          if (e.code == UV_OK) {
            return std::string(buf) + ':' + std::to_string(ntohs(si->sin_port));
          }
        } else {
          // [x::]:p
          char buf[46];
          auto si = (struct sockaddr_in6*)&sa;
          uv_err_t e = uv_inet_ntop(sa.sa_family, (const void*)&si->sin6_addr, buf, 46);
          if (e.code == UV_OK) {
            return std::string("[") + buf + "]:" + std::to_string(ntohs(si->sin6_port));
          }
        }
      }
      return std::string();
    }
  }
}


#define TLS_TRACE std::cout << "[tls] " << HI_FILENAME << ":" << __LINE__ << "\n";


static const size_t tls_init_read_buf_size = 2048;

struct tls_init_job {
  channel             ch;
  channel_connect_cb  cb;
  char                read_buf[tls_init_read_buf_size];
  tls_init_job(channel c, channel_connect_cb f) : ch(c), cb(f) {
    assert((read_buf[tls_init_read_buf_size-1] = 0) == 0); // purely for assertions
  }
};


void channel::S::tls_session::init_end(error e) {
  assert(init_job != nullptr);
  init_job->cb(e, init_job->ch);
  delete init_job;
  init_job = nullptr;
}


static error tls_error() {
  unsigned long e = ERR_get_error();
  std::string msg(512, 0);
  ERR_error_string_n(e, (char*)msg.data(), 512);
  msg.pop_back(); // ditch C-string NUL
  return error(msg, static_cast<int>(e));
}


static void tls_flush_out_bio(channel::S* self) {
  // std::cout << "flush_out_bio()\n";
  BIO* out_bio = self->tls->out_bio;

  while (BIO_pending(out_bio) != 0) {
    static const size_t BUF_SIZE = 4096;
    char* p = (char*)malloc(BUF_SIZE);
    uv_write_t* req = (uv_write_t*)p;
    char* buf = p + sizeof(uv_write_t);

    int bytes_read = BIO_read(out_bio, buf, BUF_SIZE);
    std::cout << "flush_out_bio(): bytes_read => " << bytes_read << "\n";
    if (bytes_read < 1) {
      // buffer is empty
      free((void*)req);
      break;
    }

    uv_buf_t uvbuf = { .base = buf, .len = static_cast<size_t>(bytes_read) };
    // req->data = (void*)self;
    int r = uv_write(req, self->_stream, &uvbuf, 1, [](uv_write_t* req, int status) {
      assert(status == 0); // TODO
      free((void*)req);
    });

    if (r != 0) {
      assert(r == 0); // TODO
      free((void*)req);
      break;
    }
  }
  // std::cout << "flush_out_bio() DONE\n";
};


static void tls_client_negotiate(channel::S* self) {
  // Negotiate the TLS handshake with a TLS server
  channel::S::tls_session& tls = *self->tls;
  std::cout << "[tls_negotiate] SSL_is_init_finished => " << SSL_is_init_finished(tls.session) << "\n";
  
  int r = SSL_connect(tls.session);
  std::cout << "[tls_negotiate] SSL_connect => " << r << "\n";

  if (r < 0) {
    // Still negotiating the connection
    if (SSL_get_error(tls.session, r) == SSL_ERROR_WANT_READ) {
      tls_flush_out_bio(self);
    } else {
      tls.init_end(tls_error());
    }
  } else {
    // TLS/SSL session has been established
    uv_read_stop(self->_stream);
    tls.init_end();
  }
};


static void tls_client_init(const channel& ch) {
  channel::S::tls_session& tls = *ch->self->tls;

  // prepare SSL object to work in client mode
  SSL_set_connect_state(tls.session);

  // Initiate TLS/SSL handshake
  int r = SSL_do_handshake(tls.session);
  if (r != 1) {
    // r=0 The TLS/SSL handshake was not successful but was shut down controlled and by the
    //     specifications of the TLS/SSL protocol. Call SSL_get_error() with the return value ret
    //     to find out the reason.
    //
    // r=1 The TLS/SSL handshake was successfully completed, a TLS/SSL connection has been
    //     established.
    //
    // r<0 The TLS/SSL handshake was not successful because a fatal error occurred either at the
    //     protocol level or a connection failure occurred. The shutdown was not clean. It can also
    //     occur of action is need to continue the operation for non-blocking BIOs. Call
    //     SSL_get_error() with the return value ret to find out the reason.
    
    if (SSL_get_error(tls.session, r) == SSL_ERROR_WANT_READ) {
      tls_flush_out_bio(ch->self);
    } else {
      // Actual error
      tls.init_end(tls_error());
      return;
    }

    // int e = SSL_get_error(tls.session, r);
    // std::cout << "r=" << r << ", e=" << e << "\n";
    // tls.init_end(tls_error());
    // return;
  }
  
  TLS_TRACE
  tls_client_negotiate(ch->self);

  // Start reading
  assert(ch->self->_rctx.active() == false);
  assert(ch->self->_stream != 0);
  r = uv_read_start(ch->self->_stream,

    // Allocate new buffer
    [](uv_handle_t *handle, size_t suggested_size) -> uv_buf_t {
      // return uv_buf_init((char*)malloc(2048), 2048);
      channel::S* self = static_cast<channel::S*>(handle->data);
      // Note: We are writing in sequence to BIO, so we have a single buffer that we reuse
      char* buf = self->tls->init_job->read_buf;
      size_t z = tls_init_read_buf_size;
      assert(buf[--z] == 0 && (buf[z] = 1)); // exclusive access marker
      return uv_buf_init(buf, z);
    },

    // When data is ready to be read
    [](uv_stream_t* stream, ssize_t nread, uv_buf_t buf) {
      channel::S* self = static_cast<channel::S*>(stream->data);
      channel::S::tls_session& tls = *self->tls;
      std::cout << "[tls/negotiate/read] " << nread << "\n";
      switch (nread) {
        case -1: {
          uv_read_stop(stream);
          // if (uv_last_error(self->_stream->loop).code != UV_EOF) {
          // We handle EOF as an error here, since we're still not logically connected
          tls.init_end(loop_error(stream->loop));
          break;
        }
        case 0: {
          // no-op.
          // Note that nread might also be 0, which does *not* indicate an error or
          // eof; it happens when libuv requested a buffer through the alloc callback
          // but then decided that it didn't need that buffer.
          break;
        }
        default: {
          int written = BIO_write(tls.in_bio, buf.base, nread);
          std::cout << "[tls/read] BIO_write => " << written << "\n";
          assert(written >= 0); // since its in memory-mode writing should never fail
          assert(tls.is_initiated() == false);
          tls_client_negotiate(self);
          break;
        }
      } // switch (nread)

      assert(buf.base[tls_init_read_buf_size-1] == 1); // exclusive access marker
      assert((buf.base[tls_init_read_buf_size-1] = 0) == 0); // exclusive access marker
    }
  );

  if (r != 0) {
    // uv_read_start error
    tls.init_end(loop_error(ch->self->_stream->loop));
  }
}


struct connect_job {
  channel             ch;
  channel_connect_cb  cb;
  uv_connect_t        req;
  connect_job(channel c, channel_connect_cb f) : ch(c), cb(f) {}
};


static void connect_tcp_on_open(uv_connect_t* req, int status) {
  connect_job* job = static_cast<connect_job*>(req->data);
  if (status != 0) {
    if (uv_last_error(req->handle->loop).code == UV_ECANCELED) {
      // Connection was canceled. Don't invoke callback.
    } else {
      // Error occured
      job->cb(loop_error(req->handle->loop), job->ch);
    }
  } else {
    if (job->ch->self->tls != nullptr) {
      // TLS, please
      assert(job->ch->self->tls->init_job == nullptr);
      job->ch->self->tls->init_job = new tls_init_job(job->ch, job->cb);
      tls_client_init(job->ch);
    } else {
      // We are connected
      job->cb(nullptr, job->ch);
    }
  }
  delete job;
}


static void connect_tcp_step2(const queue& q, channel ch, struct sockaddr* sa,
                              channel_connect_cb cb) {
  uv_loop_t* loop = q->_impl->loop;
  uv_tcp_t* tcp_stream = new uv_tcp_t;
  tcp_stream->data = ch.self;
  ch.self->_stream = (uv_stream_t*)tcp_stream;

  int r = uv_tcp_init(loop, tcp_stream);
  if (r != 0) {
    delete (uv_tcp_t*)ch.self->_stream; ch.self->_stream = 0;
    cb(loop_error(loop), ch);
    return;
  }

  connect_job* job = new connect_job(ch, cb);
  job->req.data = job;

  if (sa->sa_family == AF_INET) {
    r = uv_tcp_connect(&job->req, tcp_stream, *((struct sockaddr_in*)sa), connect_tcp_on_open);
  } else {
    assert(sa->sa_family == AF_INET6);
    r = uv_tcp_connect6(&job->req, tcp_stream, *((struct sockaddr_in6*)sa), connect_tcp_on_open);
  }

  if (r != 0) {
    delete job;
    delete (uv_tcp_t*)ch.self->_stream; ch.self->_stream = 0;
    cb(loop_error(loop), ch);
  }
}


struct dns_res_job {
  uv_getaddrinfo_t req;
  fun<void(error, struct sockaddr*)> cb;
};


static void dns_on_resolve(uv_getaddrinfo_t *req, int status, struct addrinfo* ai) {
  dns_res_job* job = static_cast<dns_res_job*>(req->data);
  job->cb( (status == 0 && ai != 0 ? nullptr : loop_error(req->loop)), ai == 0 ? 0 : ai->ai_addr);
  if (ai != 0) { uv_freeaddrinfo(ai); }
  delete job;
}


static error channel_parse_uri_host_port(const std::string& uri, std::string& name, std::string& port) {
  // "name:port" | "[name]:port"
  size_t p = uri.find_last_of(':');
  if (p == std::string::npos || p == uri.size()-1) {
    return error("Invalid URI: no port");
  }
  port.replace(0, port.size(), uri, p+1, uri.size()-p+1);
  if (uri[0] == '[') {
    size_t e = uri.find_first_of(']');
    if (e == std::string::npos || e == uri.size()-1) {
      return error("Invalid URI: malformed IPv6 hostname");
    }
    name.replace(0, name.size(), uri, 1, e-1);
  } else {
    name.replace(0, name.size(), uri, 0, p);
  }
  return nullptr;
}


static void connect_tcp(const queue& q, channel ch, const std::string& endpoint,
                        channel_connect_cb cb) {
  // parse hostname and port from endpoint URI
  std::string hostname, port;
  error err = channel_parse_uri_host_port(endpoint, hostname, port);
  if (err != nullptr) {
    cb(err, ch);
    return;
  }

  // DNS request job
  dns_res_job* job = new dns_res_job;
  job->req.data = (void*)job;
  job->cb = [=](error e, struct sockaddr* sa) {
    if (e != nullptr) {
      if (e.code() == UV_ENOENT) {
        e = error(std::string("Unknown hostname \"") + hostname + '"', UV_ENOENT);
      }
      cb(e, nullptr);
    } else {
      connect_tcp_step2(q, ch, sa, cb);
    }
  };

  // Dispatch
  // TODO: check q->is_current() and if not, we have to do this in the appropriate queue
  int r = uv_getaddrinfo(q->_impl->loop, &job->req, &dns_on_resolve, hostname.c_str(), port.c_str(),
                         0);
  if (r != 0) {
    delete job;
    cb(loop_error(q->_impl->loop), ch);
  }
}


static error channel_parse_uri_type(const std::string& uri, channel_type& t, std::string& rest) {
  size_t p = uri.find_first_of(':');
  if (p == std::string::npos || p == uri.size()-1) {
    return error("Invalid URI: missing protocol");
  }
  if (uri.compare(0, p, "tcp") == 0) {
    t = channel_type::TCP;
  } else {
    return error(std::string("Invalid URI protocol '" + uri.substr(0,p) + "'"));
  }
  rest.replace(0, rest.size(), uri, p+1, uri.size()-p+1);
  return nullptr;
}


channel channel::connect(queue q, const std::string& e, channel_connect_cb f) {
  return connect(q, e, nullptr, f); }

channel channel::connect(const std::string& e, channel_connect_cb f) {
  return connect(nullptr, e, nullptr, f); }

channel channel::connect(const std::string& e, tls_context s, channel_connect_cb f) {
  return connect(nullptr, e, s, f); }

channel channel::connect(queue q, const std::string& endpoint, tls_context s,
                         channel_connect_cb cb) {
  if (q == nullptr) {
    q = hi::main_queue();
  }

  // parse type from endpoint of format "type:"
  channel_type t;
  std::string endpoint2;
  error err = channel_parse_uri_type(endpoint, t, endpoint2);
  if (err != nullptr) {
    cb(err, nullptr);
    return nullptr;
  }

  channel ch(new channel::S(t, q));

  if (s != nullptr) {
    ch.self->tls = new S::tls_session(s);
  }

  switch (ch.self->_type) {
    case channel_type::TCP: { connect_tcp(ch.self->_q, ch, endpoint2, cb); break; }
  }
  return ch;
}


struct close_job {
  uv_connect_t  req;
  fun<void()>   cb;
};


void channel::close(fun<void()> cb) const {
  self->_rctx.stop();

  // Steal handle from channel
  assert(self->_stream != nullptr);
  uv_handle_t* handle = (uv_handle_t*)self->_stream;
  self->_stream = nullptr;

  // Issue `close`, eventually freeing the handle
  close_job* job = new close_job;
  job->cb = cb;
  handle->data = job;

  uv_close(handle, [](uv_handle_t* handle) {
    close_job* job = static_cast<close_job*>(handle->data);
    // delete handle;
    if ((bool)job->cb) { job->cb(); }
    delete job;
  });
}


// bool channel::is_open() const {
//   return (self->_stream != 0)
//       && (uv_is_closing((uv_handle_t*)self->_stream) == 0)
//       && ((uv_is_readable(self->_stream) != 0) ||
//           (uv_is_writable(self->_stream) != 0) );
// }


// About uv_buf_t:
//
//   struct uv_buf_t { char* base; size_t len; };
//   struct uv_buf_t { size_t len; char* base; };
//
// Due to platform differences the user cannot rely on the ordering of the
// base and len members of the uv_buf_t struct. The user is responsible for
// freeing base after the uv_buf_t is done. Return struct passed by value.

void channel::read(size_t max_size, channel_read_cb cb) const {
  assert(self->_rctx.active() == false);
  assert(self->_stream != 0);
  assert(uv_is_closing((uv_handle_t*)self->_stream) == 0);
  assert(self->tls == nullptr || self->tls->is_initiated() == true);

  self->_rctx.begin(this, cb, max_size);

  int r = uv_read_start(self->_stream,

    // Allocate new buffer
    [](uv_handle_t *handle, size_t suggested_size) -> uv_buf_t {
      channel::S* self = static_cast<channel::S*>(handle->data);
      size_t size = HI_MIN(suggested_size, self->_rctx.max_size);
      return uv_buf_init((char*)malloc(size), size); // TODO: Free list
      // The callee is responsible for freeing the buffer, libuv does not reuse it.
    },

    // When data is ready to be read
    [](uv_stream_t* stream, ssize_t nread, uv_buf_t buf) {
      channel::S* self = static_cast<channel::S*>(stream->data);

      // `nread` is > 0 if there is data available, 0 if libuv is done reading for now or -1 on
      // error.
      // printf("read(nread=%zd)\n", nread);
      switch (nread) {

        case -1: {
          // Error details can be obtained by calling uv_last_error(). UV_EOF indicates
          // that the stream has been closed.
          //
          // The callee is responsible for closing the stream when an error happens.
          // Trying to read from the stream again is undefined.
          error err;

          if (uv_last_error(stream->loop).code != UV_EOF) {
            err = loop_error(stream->loop);
            // std::cout << "read(): error: " << self->_rctx.st << "\n";
          }

          self->_rctx.cb(err, data());
          self->_rctx.end();
          break;
        }

        case 0: {
          // Note that nread might also be 0, which does *not* indicate an error or
          // eof; it happens when libuv requested a buffer through the alloc callback
          // but then decided that it didn't need that buffer.
          break;
        }

        default: {
          assert(nread > 0);
          // Note: `nread` might be less than `buf.len`
          data d = create_data(buf.base, nread, buf.len);

          if (self->tls != nullptr) {
            // TLS is active - filter through BIO
            int written = BIO_write(self->tls->in_bio, d->bytes(), d->size());
            assert(written >= 0); // since its in memory-mode writing should never fail
            std::cout << "[read] BIO_write => " << written << "\n";
            // Cause data in in_bio to be decoded and placed back into `d`
            int nread2 = SSL_read(self->tls->session, d->bytes(), d->capacity());
            std::cout << "[read] SSL_read(*, " << d->capacity() << ") => " << nread2 << "\n";
            if (nread2 < 0) {
              self->_rctx.cb(tls_error(), d);
              self->_rctx.end();
              break;
            } else if (nread2 == 0) {
              // Can happen with TLS/SSL control data. Just ignore.
              break;
            } else {
              d->set_size((size_t)nread2);
            }
          }

          // Call read handler with data in `d`
          if (!self->_rctx.cb(nullptr, d)) {
            self->_rctx.end();
          }
          break;
        }
      } // switch (nread)

    }
  );

  if (r != 0) {
    // uv_read_start error
    self->_rctx.cb(loop_error(self->_stream->loop), data());
    self->_rctx.end();
  }
}


void channel::write(const char* bytes, size_t len, fun<void(error)> cb) const {
  assert(self->_stream != 0);
  assert(uv_is_closing((uv_handle_t*)self->_stream) == 0);
  assert(self->tls == nullptr || self->tls->is_initiated() == true);

  static struct cb_ref_s { fun<void(error)> cb; } cb_ref;

  struct job_s {
    fun<void(error)>  cb;
    uv_write_t        req;
    uv_loop_t*        loop;
    char              buf[];
    void free() { cb = nullptr; ::free((void*)this); }
    static job_s* create(size_t bufsize) {
      job_s* job = (job_s*)malloc(sizeof(job_s) + bufsize);
      memcpy((void*)job, (const void*)&cb_ref, sizeof(cb_ref_s)); // init `cb`
      return job;
    }
  }* job = nullptr;

  if (self->tls != nullptr) {
    // TLS is active - filter through BIO
    TLS_TRACE
    int r = SSL_write(self->tls->session, bytes, len);
    if (r < 0) {
      if ((bool)cb) { cb(tls_error()); }
      return;
    }
    BIO* out_bio = self->tls->out_bio;
    len = BIO_pending(out_bio);
    job = job_s::create(len);
    int nread = BIO_read(out_bio, job->buf, len);
    assert(nread >= 0); // since out_bio is in memory-mode, reading should never fail
    len = static_cast<size_t>(nread);
  } else {
    job = job_s::create(len);
    memcpy((void*)job->buf, (const void*)bytes, len);
  }

  job->loop = self->_stream->loop;
  job->cb = cb;
  job->req.data = job;

  uv_buf_t uvbuf = { .base = job->buf, .len = len };

  int r = uv_write(&job->req, self->_stream, &uvbuf, 1, [](uv_write_t* req, int status) {
    job_s* job = static_cast<job_s*>(req->data);
    if ((bool)job->cb) { job->cb((status == 0) ? nullptr : loop_error(job->loop)); }
    job->free();
  });

  if (r != 0) {
    if ((bool)cb) { cb(loop_error(self->_stream->loop)); }
    job->free();
  }
}


void channel::write(char* bytes, size_t len, size_t capacity, fun<void(error)> cb) const {
  assert(self->_stream != 0);
  assert(uv_is_closing((uv_handle_t*)self->_stream) == 0);
  assert(self->tls == nullptr || self->tls->is_initiated() == true);

  struct job_s {
    uv_write_t        req;
    uv_loop_t*        loop;
    fun<void(error)>  cb;
    char*             spill_buf = nullptr;
    job_s(uv_loop_t* l, fun<void(error)> f, char* sb) : loop(l), cb(f), spill_buf(sb) {
      req.data = this; }
    ~job_s() { if (spill_buf != nullptr) { delete[] spill_buf; } }
  }* job = nullptr;


  if (self->tls != nullptr) {
    TLS_TRACE
    // TLS is active - filter through BIO
    int r = SSL_write(self->tls->session, bytes, len);
    if (r < 0) {
      cb(tls_error());
      return;
    }

    TLS_TRACE
    BIO* out_bio = self->tls->out_bio;
    size_t npending = BIO_pending(out_bio);

    if (npending > capacity) {
      TLS_TRACE
      // there's more data to be sent than we have room for in `bytes`
      capacity = npending;
      bytes = new char[capacity];
      job = new job_s(self->_stream->loop, cb, bytes);
    }

    int nread = BIO_read(out_bio, bytes, capacity);
    assert(nread >= 0); // since out_bio is in memory-mode, reading should never fail
    len = static_cast<size_t>(nread);
  }


  if (job == nullptr) {
    job = new job_s(self->_stream->loop, cb, nullptr);
  }

  uv_buf_t uvbufs[] = {{ .base = bytes, .len = len }};

  int r = uv_write(&job->req, self->_stream, uvbufs, HI_COUNTOF(uvbufs),
    [](uv_write_t* req, int status) {
      job_s* job = static_cast<job_s*>(req->data);
      job->cb((status == 0) ? nullptr : loop_error(job->loop));
      delete job;
    }
  );

  if (r != 0) {
    // uv_write error
    delete job;
    cb(loop_error(self->_stream->loop));
  }
}


// ------------------------------------------------------------------------------------------------
//                                             data
// ------------------------------------------------------------------------------------------------
#pragma mark - data

// struct data_free_list_ {
//   std::stack<data> stack;
//   size_t           sum_size;
// } data_free_list;

data create_data(char* buffer, size_t size, size_t capacity) {
  // if (q->data_pool.empty()) {
  #if 0
  data d = std::make_shared<data_>(size, buffer);
  std::cout << "*data '" << std::string((const char*)buffer, size) << "' @" << (void*)d.get() << "\n";
  return d;
  #else
  return std::make_shared<data_>(buffer, size, capacity);
  #endif
  // } else {
  //   data d = q->data_pool.front();
  //   q->data_pool.pop_front();
  //   q->data_pool.sum_size -= d.size();
  //   return d
  // }
}


data_::~data_() {
  // static const size_t BYTE_SIZE_LIMIT = 100;
  // size_t new_sum_size = q->data_free_list.sum_size + _size;
  // if (new_sum_size <= BYTE_SIZE_LIMIT) {
  //   q->data_pool.sum_size += d.size();
  //   q->push_front()
  // }
  if (_bytes != 0) {
    // std::cout << "~data '" << std::string((const char*)_bytes, _size) << "' @" << (void*)this << "\n";
    ::free((void*)_bytes);
  }
  // else { std::cerr << "~data @ '' " << (void*)this << "\n"; }
}


} // namespace
