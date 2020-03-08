const { compile, evaluate, save } = just.require('./wasm.js')

async function main () {
  const { wasm } = await compile('./fizzbuzz.wat')
  save('./fizzbuzz.wasm', wasm)
  const context = {
    println: (off, len) => just.print(context.memory.buffer.readString(len, off))
  }
  const { fizzbuzz } = evaluate(wasm, context)
  fizzbuzz(16)
  just.print(just.memoryUsage().rss)
}

main()
