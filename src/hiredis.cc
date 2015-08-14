#include "reader.h"

using namespace v8;

extern "C" {
    static NAN_MODULE_INIT(init) {
        hiredis::Reader::Initialize(target);
    }
    NODE_MODULE(hiredis, init)
}
