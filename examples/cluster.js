function threadMain () {
  const shared = just.buffer
  const million = 1024 * 1024
  const tid = just.thread.self()
  const u8 = new Uint8Array(shared)
  const u16 = new Uint16Array(shared)
  const blockSize = Atomics.load(u16, 1)
  just.print(`thread ${tid} running, blockSize ${blockSize} MB`)
  const b = new ArrayBuffer(blockSize * (million))
  const dv = new DataView(b)
  const start = Date.now()
  for (let i = 0; i < b.byteLength; i++) {
    dv.setUint8(i, 1)
  }
  const elapsed = Date.now() - start
  const bps = Math.floor((b.byteLength / (elapsed / 1000) / million * 100)) / 100
  Atomics.store(u8, 0, 1)
  just.print(`thread ${tid} done. ${bps}`)
}

const { spawn } = just.thread
const { setInterval, setTimeout, memoryUsage, cpuUsage } = just
const { cpus } = just.sys

function getSource (fn) {
  const source = fn.toString()
  return source.slice(source.indexOf('{') + 1, source.lastIndexOf('}')).trim()
}

function createThread (fn) {
  const source = getSource(fn)
  const shared = new SharedArrayBuffer(4)
  const u8 = new Uint8Array(shared)
  const u16 = new Uint16Array(shared)
  Atomics.store(u8, 0, 0)
  Atomics.store(u16, 1, blockSize)
  const tid = spawn(source, shared)
  return { shared, tid, u8 }
}

function onTimer2 () {
  if (running) {
    threads = threads.filter(t => !Atomics.load(t.u8, 0))
    if (!threads.length) {
      running = false
      const elapsed = (Date.now() - time) / 1000
      const MBytes = numThreads * blockSize
      const mbps = Math.floor((MBytes / elapsed) * 100) / 100
      just.print(`complete ${elapsed} numThreads ${numThreads} blockSize ${blockSize} average ${mbps}`)
      setTimeout(start, 5000)
    }
  }
}

function onTimer () {
  const mem = memoryUsage()
  const { user, system } = cpuUsage()
  const upc = ((user - last.user) / 1000000).toFixed(2)
  const spc = ((system - last.system) / 1000000).toFixed(2)
  just.print(`mem ${mem.rss} external ${mem.external_memory} heap ${mem.used_heap_size} threads ${threads.length} cpu ${upc} / ${spc}`)
  last.user = user
  last.system = system
}

function start () {
  time = Date.now()
  for (let i = 0; i < numThreads; i++) {
    threads.push(createThread(threadMain))
  }
  running = true
}

let time = Date.now()
const numThreads = parseInt(just.args[2] || cpus, 10)
const blockSize = parseInt(just.args[3] || 256, 10)
let running = false
let threads = []
const last = { user: 0, system: 0 }
setInterval(onTimer, 1000)
setInterval(onTimer2, 10)
start()
