const { evaluate, load } = just.require('wasm')
const wasm = load('./fizzbuzz.wasm')
const context = {
  println: (off, len) => just.print(context.memory.buffer.readString(len, off))
}
const { fizzbuzz } = evaluate(wasm, context)
fizzbuzz(16)
just.print(just.memoryUsage().rss)
