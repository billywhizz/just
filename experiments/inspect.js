const defaultIgnore = ['length', 'name', 'arguments', 'caller', 'constructor']

const ANSI_RED = '\u001b[31m'
const ANSI_MAGENTA = '\u001b[35m'
const ANSI_DEFAULT = '\u001b[0m'
const ANSI_CYAN = '\u001b[36m'
const ANSI_GREEN = '\u001b[32m'
const ANSI_WHITE = '\u001b[37m'
const ANSI_YELLOW = '\u001b[33m'

/* eslint-disable */
String.prototype.magenta = function (pad) { return `${ANSI_MAGENTA}${this.padEnd(pad, ' ')}${ANSI_DEFAULT}` }
String.prototype.red = function (pad) { return `${ANSI_RED}${this.padEnd(pad, ' ')}${ANSI_DEFAULT}` }
String.prototype.green = function (pad) { return `${ANSI_GREEN}${this.padEnd(pad, ' ')}${ANSI_DEFAULT}` }
String.prototype.cyan = function (pad) { return `${ANSI_CYAN}${this.padEnd(pad, ' ')}${ANSI_DEFAULT}` }
String.prototype.white = function (pad) { return `${ANSI_WHITE}${this.padEnd(pad, ' ')}${ANSI_DEFAULT}` }
String.prototype.yellow = function (pad) { return `${ANSI_YELLOW}${this.padEnd(pad, ' ')}${ANSI_DEFAULT}` }
/* eslint-enable */

function inspect (o, indent = 0, max = 100, ignore = defaultIgnore, lines = [], colors = 'off') {
  if (indent === max) return lines
  try {
    if (typeof o === 'object') {
      const props = Object.getOwnPropertyNames(o).filter(n => (ignore.indexOf(n) === -1))
      const hasConstructor = o.hasOwnProperty('constructor') // eslint-disable-line
      props.forEach(p => {
        if (p === 'global' || p === 'globalThis') return
        if (typeof o[p] === 'object') {
          lines.push(`\n${p.padStart(p.length + (indent * 2), ' ').cyan(0)}`, false)
        } else if (typeof o[p] === 'function') {
          if (hasConstructor) {
            lines.push(`\n${p.padStart(p.length + (indent * 2), ' ').yellow(0)}`, false)
          } else {
            lines.push(`\n${p.padStart(p.length + (indent * 2), ' ').magenta(0)}`, false)
          }
        } else {
          lines.push(`\n${p.padStart(p.length + (indent * 2), ' ').green(0)}`, false)
        }
        inspect(o[p], indent + 1, max, ignore, lines)
      })
    } else if (typeof o === 'function') {
      const props = Object.getOwnPropertyNames(o).filter(n => (ignore.indexOf(n) === -1))
      const hasConstructor = o.hasOwnProperty('constructor') // eslint-disable-line
      props.forEach(p => {
        if (p === 'prototype') {
          inspect(o[p], indent + 1, max, ignore, lines)
        } else {
          if (typeof o[p] === 'object') {
            lines.push(`\n${p.padStart(p.length + (indent * 2), ' ').cyan(0)}`, false)
          } else if (typeof o[p] === 'function') {
            if (hasConstructor) {
              lines.push(`\n${p.padStart(p.length + (indent * 2), ' ').yellow(0)}`, false)
            } else {
              lines.push(`\n${p.padStart(p.length + (indent * 2), ' ').magenta(0)}`, false)
            }
          } else {
            lines.push(`\n${p.padStart(p.length + (indent * 2), ' ').green(0)}`, false)
          }
          inspect(o[p], indent + 1, max, ignore, lines)
        }
      })
    } else {
      lines.push(`: ${o.toString()}`, false)
    }
  } catch (err) {}
  return lines
}

global.inspect = inspect

just.require('repl').repl(loop, new ArrayBuffer(4096))
