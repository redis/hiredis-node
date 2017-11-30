#include <string.h>
#include <assert.h>
#include "reader.h"

using namespace hiredis;

Reader::Reader(const Napi::CallbackInfo& info) : Napi::ObjectWrap<Database>(info) {}

Reader::~Reader() {}

Napi::Object Reader::Initialize(Napi::Env env, Napi::Object exports) {
    Napi::Function tpl = DefineClass(env, "Reader", {
      InstanceMethod("get", &Reader::Get),
      InstanceMethod("feed", &Reader::Feed)
    }); 
    constructor = Napi::Persistent(tpl);
    constructor.SuppressDestruct();
    exports.Set("Reader", tpl);
    return exports;
}

Napi::Value Reader::Feed(const Napi::CallbackInfo& info) {
  return info.Env().Undefined();
}

Napi::Value Reader::Get(const Napi::CallbackInfo& info) {
    return info.Env().Undefined();
}
