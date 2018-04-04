#include <string.h>
#include <assert.h>
#include "reader.h"
#include <iostream>

namespace hiredis {

    static void *tryParentize(const redisReadTask *task, const Napi::Value &v) { 
        Napi::HandleScope scope(v.Env());
        Reader *r = reinterpret_cast<Reader*>(task->privdata);
        size_t pidx, vidx;
        if (task->parent != NULL) {
            pidx = (size_t)task->parent->obj;
            assert(pidx > 0 && pidx < 9);

            // When there is a parent, it should be an array. 
            Napi::Value lvalue = Napi::Value(scope.Env(), r->handle[pidx].Value());
            assert(lvalue.IsArray());
            Napi::Array larray = lvalue.As<Napi::Array>();
            larray.Set(task->idx, v);

            // Store the handle when this is an inner array. Otherwise, hiredis
            // doesn't care about the return value as long as the value is set in
            // its parent array. 
            vidx = pidx+1;
            if (v.IsArray()) {
                r->handle[vidx].Reset(v.ToObject());
                return (void*)vidx;
            } else {
                // Return value doesn't matter for inner value, as long as it is 
                // not NULL (which means OOM for hiredis). 
                return (void*)0xcafef00d;
            }
        } else {   
            r->handle[1].Reset(v.ToObject()); 
            return (void*)1;
        } 
    }

    static void *createArray(const redisReadTask *task, int size) {
        Reader *r = reinterpret_cast<Reader*>(task->privdata);
        Napi::HandleScope scope(r->Env()); 
        return tryParentize(task, Napi::Array::New(scope.Env(), size));
    }

    static void *createString(const redisReadTask *task, char *str, size_t len) {
        Reader *r = reinterpret_cast<Reader*>(task->privdata);
        Napi::HandleScope scope(r->Env()); 
        Napi::Value v(r->createString(str, len));
        if (task->type == REDIS_REPLY_ERROR) {
            std::cout << "ERRORE " << REDIS_REPLY_ERROR << std::endl;
            //return tryParentize(task, Napi::Error::New(scope.Env(), v.ToString()));
        }
        return tryParentize(task, v);
    }

    static void *createInteger(const redisReadTask *task, long long value) {
        Reader *r = reinterpret_cast<Reader*>(task->privdata);
        Napi::HandleScope scope(r->Env()); 
        return tryParentize(task, Napi::Number::New(scope.Env(), value));
    }

    static void *createNil(const redisReadTask *task) {
        Reader *r = reinterpret_cast<Reader*>(task->privdata);
        Napi::HandleScope scope(r->Env()); 
        return tryParentize(task, scope.Env().Null());
    }

    static redisReplyObjectFunctions v8ReplyFunctions = {
        createString,
        createArray,
        createInteger,
        createNil,
        0 // No free function: cleanup is done in Reader::Get. 
    };

    Napi::FunctionReference Reader::constructor;

    Reader::Reader(const Napi::CallbackInfo& info) : Napi::ObjectWrap<Reader>(info) {
    this->return_buffers = false;
    if (info.Length() > 0 && info[0].IsObject()) {
        Napi::Value bv = info[0].As<Napi::Object>().Get("return_buffers");
        if (bv.IsBoolean()) {
            this->return_buffers = bv;
        }
    }
    this->reader = redisReaderCreateWithFunctions(&v8ReplyFunctions);
    this->reader->privdata = this;
    }

    Reader::~Reader() {
    redisReaderFree(this->reader);
    }

    /* Don't declare an extra scope here, so the objects are created within the
    * scope inherited from the caller (Reader::Get) and we don't have to the pay
    * the overhead. */
    inline Napi::Value Reader::createString(char *str, size_t len) {
        if (this->return_buffers) { 
            return Napi::Buffer<char>::Copy(this->Env(), str, len);
        } else {
            return Napi::String::New(this->Env(), str, len);
        }
    }

    Napi::Object Reader::Initialize(Napi::Env env, Napi::Object exports) {
        Napi::HandleScope scope(env);
        Napi::Function tpl = DefineClass(env, "Reader", {
            InstanceMethod("get", &Reader::Get),
            InstanceMethod("feed", &Reader::Feed)
        }); 
        constructor = Napi::Persistent(tpl);
        constructor.SuppressDestruct();
        exports.Set("Reader", tpl);
        return exports;
    }

    void Reader::Feed(const Napi::CallbackInfo& info) {
        Napi::Env env = info.Env();
        if (info.Length() == 0) {
            throw Napi::TypeError::New(env, "First argument must be a string or buffer");
        } else {
            if (info[0].IsBuffer()) {
                Napi::Buffer<char> buffer = info[0].As<Napi::Buffer<char>>();
                char *data = buffer.Data();
                size_t length = buffer.Length();
                /* Can't handle OOM for now. */
                assert(redisReaderFeed(this->reader, data, length) == REDIS_OK);
            } else if (info[0].IsString()) {
                std::string input = info[0].As<Napi::String>().Utf8Value();
                const char *str = input.c_str();
                redisReplyReaderFeed(this->reader, str, input.length());
            } else {
                throw Napi::Error::New(env, "invalid argument");
            }
        }
        //return info.This();
    }

    Napi::Value Reader::Get(const Napi::CallbackInfo& info) {
        Napi::Env env = info.Env();
        void *index = NULL;
        Napi::Value reply;
        int i;
        if (redisReaderGetReply(this->reader, &index) == REDIS_OK) {
            if (index == 0) {
                return env.Undefined();
            } else {
                // Complete replies should always have a root object at index 1.
                assert((size_t)index == 1); 
                reply = Napi::Value(env, this->handle[1].Value());
                // Dispose and clear used handles.
                for (i = 1; i < 3; i++) {
                    this->handle[i].Reset();
                }
            }
        } else {
            throw Napi::Error::New(env, this->reader->errstr);
        }
        return reply;
    }
}
