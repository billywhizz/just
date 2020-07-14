const source = just.fs.readFile('parse.js')
const { spawn, join } = just.thread
const cpus = parseInt(just.args[2] || just.sys.cpus, 10)
const threads = []
for (let i = 0; i < cpus; i++) {
  threads.push(spawn(source))
}
threads.forEach(tid => join(tid))
