var assert = require("assert"),
    test = require("./testlib")(),
    hiredis = require("../hiredis");

test("WriteCommand", function() {
    var reader = new hiredis.Reader();
    reader.feed(hiredis.writeCommand("hello", "world"));
    assert.deepEqual(["hello", "world"], reader.get());
});
