var hiredis = require("./hiredis"),
    num_clients = 10,
    clients = new Array(num_clients),
    active_clients = 0,
    num_requests = parseInt(process.argv[2]) || 20000,
    issued_requests = 0,
    test_start;

function create_clients() {
    var i = num_clients;
    while(i--) {
        clients[i] = hiredis.createConnection();
    }
}

var tests = [];
tests.push({
    descr: "PING",
    command: ["PING"]
});
tests.push({
    descr: "SET",
    command: ["SET", "foo", "bar"]
});
tests.push({
    descr: "GET",
    command: ["GET", "foo"]
});
tests.push({
    descr: "LPUSH 8 bytes",
    command: ["LPUSH", "mylist-8", new Buffer(Array(8).join("-"))]
});
tests.push({
    descr: "LPUSH 64 bytes",
    command: ["LPUSH", "mylist-64", new Buffer(Array(64).join("-"))]
});
tests.push({
    descr: "LPUSH 512 bytes",
    command: ["LPUSH", "mylist-512", new Buffer(Array(512).join("-"))]
});
tests.push({
    descr: "LRANGE 0 99, 8 bytes",
    command: ["LRANGE", "mylist-8", "0", "99"]
});
tests.push({
    descr: "LRANGE 0 99, 64 bytes",
    command: ["LRANGE", "mylist-64", "0", "99"]
});
tests.push({
    descr: "LRANGE 0 99, 512 bytes",
    command: ["LRANGE", "mylist-512", "0", "99"]
});

function call(client, test) {
    var i = issued_requests++;
    client.write.apply(client,test.command);
    client.once("reply", function() {
        if (issued_requests < num_requests) {
            call(client, test);
        } else {
            client.end();
            if (--active_clients == 0)
                done(test);
        }
    });
}

function done(test) {
    var time = (new Date - test_start);
    var op_rate = (num_requests/(time/1000.0)).toFixed(2);
    console.log(test.descr + ": " + op_rate + " ops/sec");
    next();
}

function next() {
    var test = tests.shift();
    var i = num_clients;
    if (test) {
        create_clients();
        issued_requests = 0;
        test_start = new Date;
        while(i-- && issued_requests < num_requests) {
            active_clients++;
            call(clients[i], test);
        }
    }
}

next();

