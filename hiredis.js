var net = require("net"),
    hiredis = require('bindings')('hiredis.node');

var bufStar = new Buffer("*", "ascii");
var bufDollar = new Buffer("$", "ascii");
var bufCrlf = new Buffer("\r\n", "ascii");

exports.Reader = hiredis.Reader;

exports.writeCommand = function() {
    var args = arguments,
        bufLen = new Buffer(String(args.length), "ascii"),
        parts = [bufStar, bufLen, bufCrlf],
        size = 3 + bufLen.length;

    for (var i = 0; i < args.length; i++) {
        var arg = args[i];
        if (!Buffer.isBuffer(arg))
            arg = new Buffer(String(arg));

        bufLen = new Buffer(String(arg.length), "ascii");
        parts = parts.concat([
            bufDollar, bufLen, bufCrlf,
            arg, bufCrlf
        ]);
        size += 5 + bufLen.length + arg.length;
    }

    return Buffer.concat(parts, size);
}

exports.createConnection = function(port, host) {
    var s = net.createConnection(port || 6379, host);
    var r = new hiredis.Reader();
    var _write = s.write;

    s.write = function() {
        var data = exports.writeCommand.apply(this, arguments);
        return _write.call(s, data);
    }

    s.on("data", function(data) {
        var reply;
        r.feed(data);
        try {
            while((reply = r.get()) !== undefined)
                s.emit("reply", reply);
        } catch(err) {
            r = null;
            s.emit("error", err);
            s.destroy();
        }
    });

    return s;
}

