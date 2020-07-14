function threadMain () {
  just.sys.sleep(2)
}

function getSource (fn) {
  const source = fn.toString()
  return source.slice(source.indexOf('{') + 1, source.lastIndexOf('}')).trim()
}

const { spawn, join } = just.thread
const source = getSource(threadMain)
const threads = []
for (let i = 0; i < 1024; i++) {
  threads.push(spawn(source))
}
threads.forEach(tid => join(tid))
