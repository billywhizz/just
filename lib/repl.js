const { net, sys, vm } = just

const stringify = (o, sp = '  ') => JSON.stringify(o, (k, v) => (typeof v === 'bigint') ? v.toString() : v, sp)

function repl (loop, buf, stdin = sys.STDIN_FILENO, stdout = sys.STDOUT_FILENO) {
  const { EPOLLIN } = just.loop
  const { O_NONBLOCK, EAGAIN } = just.net
  sys.fcntl(stdin, sys.F_SETFL, (sys.fcntl(stdin, sys.F_GETFL, 0) | O_NONBLOCK))
  loop.add(stdin, (fd, event) => {
    if (event & EPOLLIN) {
      const bytes = net.read(fd, buf)
      if (bytes < 0) {
        const err = sys.errno()
        if (err !== EAGAIN) {
          just.print(`read error: ${sys.strerror(err)} (${err})`)
          net.close(fd)
        }
        return
      }
      const expr = buf.readString(bytes).trim()
      try {
        if (expr === '.exit') {
          loop.remove(stdin)
          loop.remove(stdout)
          net.close(stdin)
          net.close(stdout)
          return
        }
        const result = vm.runScript(expr, 'repl')
        if (result || (typeof result === 'number' || typeof result === 'boolean')) {
          // todo: buffer overruns
          net.write(stdout, buf, buf.writeString(`\x1B[0m${stringify(result, 2)}\n`))
        }
        net.write(stdout, buf, buf.writeString('\x1B[32m>\x1B[33m '))
      } catch (err) {
        net.write(stdout, buf, buf.writeString(`\x1B[0m${err.stack}\n`))
        net.write(stdout, buf, buf.writeString('\x1B[32m>\x1B[33m '))
      }
    }
  })
  loop.add(stdout, (fd, event) => {

  })
  net.write(stdout, buf, buf.writeString('\x1B[32m>\x1B[33m '))
}

module.exports = { repl }
