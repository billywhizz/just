const { fs, net } = just

const fileTypes = {}
fileTypes[fs.DT_BLK] = 'block'
fileTypes[fs.DT_CHR] = 'character'
fileTypes[fs.DT_DIR] = 'directory'
fileTypes[fs.DT_FIFO] = 'fifo'
fileTypes[fs.DT_LNK] = 'symlink'
fileTypes[fs.DT_REG] = 'regular'
fileTypes[fs.DT_SOCK] = 'socket'
const UNKNOWN = 'unknown'

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
  const st = {}
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

function readFile (path, buf = new ArrayBuffer(4096)) {
  const chunks = []
  const fd = fs.open(path, fs.O_RDONLY)
  if (fd < 0) return fd
  let len = net.read(fd, buf)
  while (len > 0) {
    chunks.push(buf.readString(len))
    len = net.read(fd, buf)
  }
  const r = net.close(fd)
  if (len < 0) return len
  if (r < 0) return r
  return chunks.join('')
}

function writeFile (path, buf) {
  const len = buf.byteLength
  if (!len) return -1
  const fd = fs.open(path, fs.O_WRONLY | fs.O_CREAT | fs.O_TRUNC)
  const chunks = Math.ceil(len / 4096)
  let total = 0
  for (let i = 0, o = 0; i < chunks; ++i, o += 4096) {
    const bytes = net.write(fd, buf.slice(o, o + 4096))
    if (bytes <= 0) return bytes
    total += bytes
  }
  const r = net.close(fd)
  if (r < 0) return r
  return total
}

module.exports = { readFile, writeFile, getStats, fileType, checkMode, checkFlag }
