setTimeout(() => {
  just.print('goodbye')
}, 1000)

let counter = 0
const timer = setInterval(() => {
  just.print('hello')
  if (counter++ === 3) {
    clearTimeout(timer)
  }
}, 1000)
