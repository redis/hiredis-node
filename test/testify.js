var assert = require("assert"),
    test = require("./testlib")(),
    hiredis = require("../hiredis");

test("StatusReplyAsBuffer", function() {
    var reader = new hiredis.Reader({ return_buffers: true });
    reader.feed("+OK\r\n");
    var reply = reader.get();
    assert.ok(Buffer.isBuffer(reply));
    assert.equal("OK", reply.toString());
});    

/*test("FeedWithBuffer", function() {
    var reader = new hiredis.Reader();
    reader.feed(new Buffer("$3\r\nfoo\r\n"));
    assert.deepEqual("foo", reader.get());
});*/

/*test("UndefinedReplyOnIncompleteFeed", function() {
    var reader = new hiredis.Reader();
    reader.feed("$3\r\nfoo");
    assert.deepEqual(undefined, reader.get());
    reader.feed("\r\n");
    assert.deepEqual("foo", reader.get());
});*/

