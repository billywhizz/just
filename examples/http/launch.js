const pids = []

pids.push(just.sys.spawn('just', just.sys.cwd(), ['httpd.js']))
pids.push(just.sys.spawn('just', just.sys.cwd(), ['httpd.js']))

just.setInterval(() => {

}, 1000)
