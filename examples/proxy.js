const { sys, net } = just
const { EPOLLIN, EPOLLOUT, EPOLLERR, EPOLLHUP } = just.loop
const { AF_INET, SOCK_STREAM, SOCK_NONBLOCK, SOMAXCONN, SOL_SOCKET, SO_REUSEADDR, SO_REUSEPORT } = net

const loop = just.createLoop(128)
const timer = sys.timer(1000, 1000)
const frontend = net.socket(AF_INET, SOCK_STREAM | SOCK_NONBLOCK, 0)
let r = net.setsockopt(frontend, SOL_SOCKET, SO_REUSEADDR, 1)
r = net.setsockopt(frontend, SOL_SOCKET, SO_REUSEPORT, 1)
r = net.bind(frontend, '127.0.0.1', 9222)
if (r !== 0) throw new Error('bind')
r = net.listen(frontend, SOMAXCONN)

loop.add(frontend, (fd, event) => {
  if (event & EPOLLIN) {
    const client = net.accept(fd)
    const backend = net.socket(AF_INET, SOCK_STREAM | SOCK_NONBLOCK, 0)
    just.print('client.connect')
    net.connect(backend, '127.0.0.1', 9333)
    loop.add(backend, (fd, event) => {
      if (event & EPOLLOUT) {
        just.print('backend.connect')
        loop.update(fd, EPOLLIN)
        loop.add(client, (fd, event) => {
          if (event & EPOLLIN) {
            const buf = new ArrayBuffer(4096)
            const bytes = net.read(fd, buf)
            just.print(`client.read: ${bytes}`)
            just.print(buf.readString(bytes))
            if (bytes === 0) {
              just.print('client.close')
              net.close(fd)
              net.close(backend)
            } else if (bytes > 0) {
              const written = net.write(backend, buf, bytes)
              just.print(`backend.write: ${written}`)
            }
          }
          if (event & EPOLLERR || event & EPOLLHUP) {
            just.print('client.close')
            net.close(fd)
            net.close(backend)
          }
        })
      }
      if (event & EPOLLIN) {
        const buf = new ArrayBuffer(4096)
        const bytes = net.read(fd, buf)
        just.print(`backend.read: ${bytes}`)
        just.print(buf.readString(bytes))
        if (bytes === 0) {
          just.print('backend.close')
          net.close(fd)
          net.close(client)
        } else if (bytes > 0) {
          const written = net.write(client, buf, bytes)
          just.print(`client.write: ${written}`)
        }
      }
      if (event & EPOLLERR || event & EPOLLHUP) {
        just.print('backend.close')
        net.close(fd)
        net.close(client)
      }
    }, EPOLLOUT)
  }
})

loop.add(timer, (fd, event) => {
  if (event & EPOLLIN) {
    net.read(fd, new ArrayBuffer(8))
    just.print(JSON.stringify(just.memoryUsage(), null, '  '))
  }
})

while (loop.count > 0) {
  loop.poll(1)
  sys.runMicroTasks()
}

net.close(loop.fd)
