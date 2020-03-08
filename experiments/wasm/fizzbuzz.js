const { compile, evaluate, save } = just.require('wasm')

async function main () {
  const fileName = just.path.join(just.path.baseName(just.path.scriptName), './fizzbuzz.wat')
  just.print(fileName)
  const { wasm } = await compile(fileName)
  save('./fizzbuzz.wasm', wasm)
  const context = {
    println: (off, len) => just.print(context.memory.buffer.readString(len, off))
  }
  const { fizzbuzz } = evaluate(wasm, context)
  fizzbuzz(16)
  just.print(just.memoryUsage().rss)
}

main().catch(err => just.error(err.stack))
