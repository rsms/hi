function http_handler(name, req, res) {
  console.log('[' + name + ']',
    '(' + req.socket.remoteAddress + ',' + req.socket.remotePort + ')',
    req.method, req.url);
  var body = 'Hello World\n';
  res.writeHead(200, {'Content-Type': 'text/plain', 'Content-length': body.length });
  res.end(body);
}

function start_http_server(addr) {
  require('http').createServer(
    function (req, res) { http_handler('http', req, res); }
  ).listen(8000, addr);
  console.log('listening (http, ' + addr + ', 8000)');
}

function start_https_server(addr) {
  var fs = require('fs');
  require('https').createServer(
    { key:  fs.readFileSync(__dirname + '/server.pem'),
      cert: fs.readFileSync(__dirname + '/server.crt'),
      passphrase: 'NmNTNA9idsq4iuzH' },
    function (req, res) { http_handler('https', req, res); }
  ).listen(4430, addr);
  console.log('listening (https, ' + addr + ', 4430)');
}

start_http_server('127.0.0.1');
start_https_server('127.0.0.1');
start_http_server('::1');
start_https_server('::1');
