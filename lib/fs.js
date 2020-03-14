const { fs, net, sys } = just

const st = {}
const fileTypes = {} // lookup for meaningful names of device types
fileTypes[fs.DT_BLK] = 'block'
fileTypes[fs.DT_CHR] = 'character'
fileTypes[fs.DT_DIR] = 'directory'
fileTypes[fs.DT_FIFO] = 'fifo'
fileTypes[fs.DT_LNK] = 'symlink'
fileTypes[fs.DT_REG] = 'regular'
fileTypes[fs.DT_SOCK] = 'socket'
const UNKNOWN = 'unknown'
const stat = new BigUint64Array(20) // single instance for file stats

function checkFlag (val, flag) {
  return (val & flag) === flag
}

function checkMode (val, mode) {
  return (val & fs.S_IFMT) === mode
}

function fileType (type) {
  return fileTypes[type] || UNKNOWN
}

function getStats (stat) {
  st.deviceId = stat[0]
  st.mode = Number(stat[1])
  st.hardLinks = stat[2]
  st.uid = stat[3]
  st.gid = stat[4]
  st.rdev = stat[5] // ?
  st.inode = stat[6]
  st.size = stat[7]
  st.blockSize = stat[8]
  st.blocks = stat[9]
  st.flags = stat[10]
  st.st_gen = stat[11] // ?
  st.accessed = { tv_sec: stat[12], tv_usec: stat[13] }
  st.modified = { tv_sec: stat[14], tv_usec: stat[15] }
  st.created = { tv_sec: stat[16], tv_usec: stat[17] }
  st.permissions = {
    user: {
      r: checkFlag(st.mode, fs.S_IRUSR),
      w: checkFlag(st.mode, fs.S_IWUSR),
      x: checkFlag(st.mode, fs.S_IXUSR)
    },
    group: {
      r: checkFlag(st.mode, fs.S_IRGRP),
      w: checkFlag(st.mode, fs.S_IWGRP),
      x: checkFlag(st.mode, fs.S_IXGRP)
    },
    other: {
      r: checkFlag(st.mode, fs.S_IROTH),
      w: checkFlag(st.mode, fs.S_IWOTH),
      x: checkFlag(st.mode, fs.S_IXOTH)
    }
  }
  st.type = {
    socket: checkMode(st.mode, fs.S_IFSOCK),
    symlink: checkMode(st.mode, fs.S_IFLNK),
    regular: checkMode(st.mode, fs.S_IFREG),
    block: checkMode(st.mode, fs.S_IFBLK),
    directory: checkMode(st.mode, fs.S_IFDIR),
    character: checkMode(st.mode, fs.S_IFCHR),
    fifo: checkMode(st.mode, fs.S_IFIFO)
  }
  return st
}

function readFileBytes (path, flags = fs.O_RDONLY) {
  const fd = fs.open(path, flags)
  if (fd < 0) return fd
  let r = fs.fstat(fd, stat)
  if (r < 0) throw new Error(`Error Stating File: ${sys.errno()}`)
  const size = Number(stat[7])
  const buf = new ArrayBuffer(size)
  let off = 0
  let len = net.read(fd, buf, off)
  while (len > 0) {
    off += len
    if (off === size) break
    len = net.read(fd, buf, off)
  }
  off += len
  r = net.close(fd)
  if (len < 0) {
    const errno = sys.errno()
    throw new Error(`Error Reading File: ${errno} (${sys.strerror(errno)})`)
  }
  if (off < size) {
    throw new Error(`Size Mismatch: size: ${size}, read: ${off}`)
  }
  if (r < 0) throw new Error(`Error Closing File: ${sys.errno()}`)
  return buf
}

function readFile (path, unused, flags = fs.O_RDONLY) {
  const buf = readFileBytes(path, flags)
  return buf.readString(buf.byteLength, 0)
}

function writeFile (path, buf, flags = fs.O_WRONLY | fs.O_CREAT | fs.O_TRUNC) {
  const len = buf.byteLength
  if (!len) return -1
  const fd = fs.open(path, flags)
  if (fd < 0) throw new Error(`Error Opening File: ${sys.errno()}`)
  const chunks = Math.ceil(len / 4096)
  let total = 0
  let bytes = 0
  for (let i = 0, off = 0; i < chunks; ++i, off += 4096) {
    const towrite = Math.min(len - off, 4096)
    bytes = net.write(fd, buf, towrite, off)
    if (bytes <= 0) break
    total += bytes
  }
  if (bytes < 0) {
    const errno = sys.errno()
    throw new Error(`Error Writing to File: ${errno} (${sys.strerror(errno)})`)
  }
  if (bytes === 0) {
    throw new Error(`Zero Bytes Written: ${sys.errno()}`)
  }
  const r = net.close(fd)
  if (r < 0) throw new Error(`Error Closing File: ${sys.errno()}`)
  return total
}

module.exports = {
  readFile,
  readFileBytes,
  writeFile,
  getStats,
  fileType,
  checkMode,
  checkFlag
}
