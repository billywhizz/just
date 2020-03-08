const { net, sys, vm } = just
const { EPOLLIN, EPOLLERR, EPOLLHUP } = just.loop

const stringify = (o, sp = '  ') => JSON.stringify(o, (k, v) => (typeof v === 'bigint') ? v.toString() : v, sp)

const ANSI_RED = '\u001b[31m'
const ANSI_MAGENTA = '\u001b[35m'
const ANSI_DEFAULT = '\u001b[0m'
const ANSI_CYAN = '\u001b[36m'
const ANSI_GREEN = '\u001b[32m'
const ANSI_WHITE = '\u001b[37m'
const ANSI_YELLOW = '\u001b[33m'

function writeString (fd, str) {
  const buf = ArrayBuffer.fromString(str)
  const len = buf.byteLength
  const chunks = Math.ceil(len / 4096)
  let total = 0
  let bytes = 0
  for (let i = 0, off = 0; i < chunks; ++i, off += 4096) {
    const towrite = Math.min(len - off, 4096)
    bytes = net.write(fd, buf, towrite, off)
    if (bytes <= 0) break
    total += bytes
  }
  if (bytes < 0) throw new Error(`Error Writing: ${sys.errno()} (${sys.strerror(sys.errno())})`)
  if (bytes === 0) throw new Error(`Zero Bytes Written: ${sys.errno()}`)
  return total
}

function repl (loop, buf, stdin = sys.STDIN_FILENO, stdout = sys.STDOUT_FILENO) {
  const { EPOLLIN } = just.loop
  const { O_NONBLOCK, EAGAIN } = just.net
  sys.fcntl(stdin, sys.F_SETFL, (sys.fcntl(stdin, sys.F_GETFL, 0) | O_NONBLOCK))
  sys.fcntl(stdin, sys.F_SETFL, (sys.fcntl(stdin, sys.F_GETFL, 0) | O_NONBLOCK))
  const current = []
  loop.add(stdin, (fd, event) => {
    if (event & EPOLLERR || event & EPOLLHUP) {
      net.close(fd)
      return
    }
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
      const expr = buf.readString(bytes)
      let lf = expr.indexOf('\r')
      if (lf === -1) lf = expr.indexOf('\n')
      if (lf > -1) {
        current.push(expr.slice(0, lf))
        const command = current.join('').trim()
        current.length = 0
        try {
          if (command === '.exit') {
            loop.remove(stdin)
            loop.remove(stdout)
            net.close(stdin)
            net.close(stdout)
            return
          }
          const result = vm.runScript(command, 'repl')
          if (result || (typeof result === 'number' || typeof result === 'boolean')) {
            writeString(stdout, `${ANSI_DEFAULT}${stringify(result, 2)}\n`)
          } else {
            if (result === null) {
              writeString(stdout, `${ANSI_RED}(null)${ANSI_DEFAULT}\n`)
            } else if (result === undefined) {
              writeString(stdout, `${ANSI_RED}(undefined)${ANSI_DEFAULT}\n`)
            }
          }
          writeString(stdout, `${ANSI_GREEN}>${ANSI_YELLOW} `)
        } catch (err) {
          writeString(stdout, `${ANSI_DEFAULT}${err.stack}\n`)
          writeString(stdout, `${ANSI_GREEN}>${ANSI_YELLOW} `)
        }
      } else {
        current.push(expr)
      }
    }
  })
  loop.add(stdout, (fd, event) => {
    // todo: pause/resume on writable
  })
  writeString(stdout, `${ANSI_GREEN}>${ANSI_YELLOW} `)
}

module.exports = { repl }
