var assert = require("assert"),
    spawn = require('child_process').spawn,
    hiredis = require("../build/default/hiredis");

exports.testCreateReader = function() {
    var reader = new hiredis.Reader();
    assert.isNotNull(reader);
}

exports.testStatusReply = function() {
    var reader = new hiredis.Reader();
    reader.feed("+OK\r\n");
    assert.equal("OK", reader.get());
}

exports.testIntegerReply = function() {
    var reader = new hiredis.Reader();
    reader.feed(":1\r\n");
    assert.equal(1, reader.get());
}

exports.testErrorReply = function() {
    var reader = new hiredis.Reader();
    reader.feed("-ERR foo\r\n");
    var reply = reader.get();
    assert.equal(Error, reply.constructor);
    assert.equal("ERR foo", reply.message);
}

exports.testNullBulkReply = function() {
    var reader = new hiredis.Reader();
    reader.feed("$-1\r\n");
    assert.equal(null, reader.get());
}

exports.testEmptyBulkReply = function() {
    var reader = new hiredis.Reader();
    reader.feed("$0\r\n\r\n");
    assert.equal("", reader.get());
}

exports.testBulkReply = function() {
    var reader = new hiredis.Reader();
    reader.feed("$3\r\nfoo\r\n");
    assert.equal("foo", reader.get());
}

exports.testBulkReplyWithEncoding = function() {
    var reader = new hiredis.Reader();
    reader.feed("$" + Buffer.byteLength("☃") + "\r\n☃\r\n");
    assert.equal("☃", reader.get());
}

exports.testNullMultiBulkReply = function() {
    var reader = new hiredis.Reader();
    reader.feed("*-1\r\n");
    assert.equal(null, reader.get());
}

exports.testEmptyMultiBulkReply = function() {
    var reader = new hiredis.Reader();
    reader.feed("*0\r\n");
    assert.eql([], reader.get());
}

exports.testMultiBulkReply = function() {
    var reader = new hiredis.Reader();
    reader.feed("*2\r\n$3\r\nfoo\r\n$3\r\nbar\r\n");
    assert.eql(["foo", "bar"], reader.get());
}

exports.testNestedMultiBulkReply = function() {
    var reader = new hiredis.Reader();
    reader.feed("*2\r\n*2\r\n$3\r\nfoo\r\n$3\r\nbar\r\n$3\r\nqux\r\n");
    assert.eql([["foo", "bar"], "qux"], reader.get());
}

exports.testMultiBulkReplyWithNonStringValues = function() {
    var reader = new hiredis.Reader();
    reader.feed("*3\r\n:1\r\n+OK\r\n$-1\r\n");
    assert.eql([1, "OK", null], reader.get());
}

exports.testFeedWithBuffer = function() {
    var reader = new hiredis.Reader();
    reader.feed(new Buffer("$3\r\nfoo\r\n"));
    assert.eql("foo", reader.get());
}

exports.testUndefinedReplyOnIncompleteFeed = function() {
    var reader = new hiredis.Reader();
    reader.feed("$3\r\nfoo");
    assert.eql(undefined, reader.get());
    reader.feed("\r\n");
    assert.eql("foo", reader.get());
}

exports.testLeaks = function(beforeExit) {
    /* The "leaks" utility is only available on OSX. */
    if (process.platform != "darwin") return;

    var done = 0;
    var leaks = spawn("leaks", [process.pid]);
    leaks.stdout.on("data", function(data) {
        var str = data.toString();
        var notice = "Node 0.2.5 always leaks 16 bytes (this is " + process.versions.node + ")";
        var matches;
        if ((matches = /(\d+) leaks?/i.exec(str)) != null) {
            if (parseInt(matches[1]) > 0) {
                console.log(str);
                console.log('\x1B[31mNotice: ' + notice + '\x1B[0m');
            }
        }
        done = 1;
    });

    beforeExit(function() {
        setTimeout(function() {
            assert.ok(done, "Leaks test should have completed");
        }, 100);
    });
}

