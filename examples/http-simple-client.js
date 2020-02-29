const { sys, net, http } = just
const BUFSIZE = 16384
const { SOCK_STREAM, AF_INET, SOL_SOCKET, IPPROTO_TCP, TCP_NODELAY, SO_KEEPALIVE } = net
const rbuf = new ArrayBuffer(BUFSIZE)
const wbuf = sys.calloc(1, 'GET / HTTP/1.1\r\n\r\n')
const fd = net.socket(AF_INET, SOCK_STREAM, 0)
net.connect(fd, '127.0.0.1', 3000)
net.setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, 1)
net.setsockopt(fd, SOL_SOCKET, SO_KEEPALIVE, 1)
const la = net.getsockname(fd, AF_INET, []);
const ra = net.getpeername(fd, AF_INET, []);
just.print(JSON.stringify(la))
just.print(JSON.stringify(ra))
net.send(fd, wbuf)
const bytes = net.recv(fd, rbuf)
just.print(bytes)
const nread = http.parseResponse(rbuf, bytes, 0)
const response = http.getResponse()
just.print(JSON.stringify(response, null, '  '))
net.close(fd)