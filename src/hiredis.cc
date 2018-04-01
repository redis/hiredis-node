#include <napi.h>
#include "reader.h"

Napi::Object Init(Napi::Env env, Napi::Object exports) {
    hiredis::Reader::Initialize(env, exports);
    return exports;
}

NODE_API_MODULE(hiredis, Init)