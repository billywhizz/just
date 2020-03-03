const pathMod = require('path')

function wrap (cache = {}) {
  function require (path, parent) {
    if (path.split('.').slice(-1)[0] !== 'js') {
      return just.requireNative(path, parent)
    }
    const { join, baseName } = pathMod
    let dirName = parent ? parent.dirName : baseName(join(just.sys.cwd(), just.args[1] || './'))
    const fileName = join(dirName, path)
    if (cache[fileName]) return cache[fileName].exports
    dirName = baseName(fileName)
    const params = ['exports', 'require', 'module']
    const exports = {}
    const module = { exports, dirName, fileName }
    module.text = just.fs.readFile(fileName)
    const fun = just.vm.compile(module.text, fileName, params, [])
    module.function = fun
    cache[fileName] = module
    fun.call(exports, exports, p => require(p, module), module)
    return module.exports
  }
  return { require, cache }
}

module.exports = { wrap }
