var hiredis = require('bindings')('hiredis')

var reader = new hiredis.Reader();

test("NullBulkReply", function() {
    var reader = new hiredis.Reader();
    reader.feed("$-1\r\n");
    assert.equal(null, reader.get());
});