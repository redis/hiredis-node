var hiredis = require('bindings')('hiredis')

var reader = new hiredis.Reader();
console.log(reader.feed)
console.log(reader.get)

reader.feed("+OK\r\n");
console.log("feed called")
console.log(reader.get())