const { print, sys, net } = just
const { strerror, errno } = sys
const { close, read } = net
const BUFSIZE = 65536
let total = 0
const stdin = 0
const rbuf = new ArrayBuffer(BUFSIZE)
while (1) {
  const bytes = read(stdin, rbuf)
  if (bytes < 0) {
    const err = errno()
    just.print(`read error: ${strerror(err)} (${err})`)
    close(stdin)
    break
  }
  total += bytes
  if (bytes === 0) {
    close(stdin)
    print(total)
    break
  }
}
