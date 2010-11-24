var assert = require("assert"),
    hiredis = require("../src/build/default/hiredis");

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
    assert.equal("ERR foo", reader.get());
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

