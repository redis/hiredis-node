module.exports = function() {
    function test(str, fn) {
        try {
            fn();
            test.passed++;
        } catch (err) {
            console.log("\x1B[1;31m" + str + " failed!\x1B[0m");
            console.log(err.stack + "\n");
            test.failed++;
        }
    }

    test.passed = 0;
    test.failed = 0;

    return test;
}
