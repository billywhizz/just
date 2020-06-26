const { net, sys } = just
const { socketpair, AF_UNIX, SOCK_STREAM } = net
const fds = []
socketpair(AF_UNIX, SOCK_STREAM, fds)
just.print(fds)
const buf = sys.calloc(1, 'Hello')
const buf2 = sys.calloc(1, 100)
net.send(fds[0], buf)
const bytes = net.recv(fds[1], buf2)
just.print(sys.readString(buf2, bytes))
