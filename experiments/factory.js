const { sys } = just
const {
  create, wait, control, EPOLL_CLOEXEC, EPOLL_CTL_ADD,
  EPOLL_CTL_DEL, EPOLL_CTL_MOD, EPOLLIN
} = just.loop

function createLoop (nevents = 1024) {
  const evbuf = new ArrayBuffer(nevents * 12)
  const events = new Uint32Array(evbuf)
  const loopfd = create(EPOLL_CLOEXEC)
  const handles = {}
  function poll (timeout = -1, sigmask) {
    let r = 0
    if (sigmask) {
      r = wait(loopfd, evbuf, timeout, sigmask)
    } else {
      r = wait(loopfd, evbuf, timeout)
    }
    if (r > 0) {
      let off = 0
      for (let i = 0; i < r; i++) {
        const fd = events[off + 1]
        handles[fd](fd, events[off])
        off += 3
      }
    }
    return r
  }
  function add (fd, callback, events = EPOLLIN) {
    const r = control(loopfd, EPOLL_CTL_ADD, fd, events)
    if (r === 0) {
      handles[fd] = callback
      instance.count++
    }
    return r
  }
  function remove (fd) {
    const r = control(loopfd, EPOLL_CTL_DEL, fd)
    if (r === 0) {
      delete handles[fd]
      instance.count--
    }
    return r
  }
  function update (fd, events = EPOLLIN) {
    const r = control(loopfd, EPOLL_CTL_MOD, fd, events)
    return r
  }
  const instance = { fd: loopfd, poll, add, remove, update, handles, count: 0 }
  return instance
}

const factory = {
  loops: [createLoop(1024)],
  paused: true,
  create: (nevents = 128) => {
    const loop = createLoop(nevents)
    factory.loops.push(loop)
    return loop
  },
  run: (ms = 1) => {
    factory.paused = false
    while (!factory.paused) {
      for (const loop of factory.loops) {
        if (loop.count > 0) loop.poll(ms)
      }
      sys.runMicroTasks()
    }
  },
  stop: () => {
    factory.paused = true
  },
  shutdown: () => {

  }
}

factory.default = global.loop = factory.loops[0]

module.exports = { factory, createLoop }
