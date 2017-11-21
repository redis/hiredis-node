#include <string.h>
#include <assert.h>
#include "reader.h"

using namespace hiredis;

static void *tryParentize(const redisReadTask *task, const Local<Value> &v) {
    Nan::HandleScope scope;

    Reader *r = reinterpret_cast<Reader*>(task->privdata);
    size_t pidx, vidx;

    if (task->parent != NULL) {
        pidx = (size_t)task->parent->obj;
        assert(pidx > 0 && pidx < 9);

        /* When there is a parent, it should be an array. */
        Local<Value> lvalue = Nan::New(r->handle[pidx]);
        assert(lvalue->IsArray());
        Local<Array> larray = lvalue.As<Array>();
        larray->Set(task->idx,v);

        /* Store the handle when this is an inner array. Otherwise, hiredis
         * doesn't care about the return value as long as the value is set in
         * its parent array. */
        vidx = pidx+1;
        if (v->IsArray()) {
            r->handle[vidx].Reset(v);
            return (void*)vidx;
        } else {
            /* Return value doesn't matter for inner value, as long as it is
             * not NULL (which means OOM for hiredis). */
            return (void*)0xcafef00d;
        }
    } else {
        /* There is no parent, so this value is the root object. */
        r->handle[1].Reset(v);
        return (void*)1;
    }
}

static void *createArray(const redisReadTask *task, int size) {
    Nan::HandleScope scope;

    return tryParentize(task, Nan::New<Array>(size));
}

static void *createString(const redisReadTask *task, char *str, size_t len) {
    Nan::HandleScope scope;

    Reader *r = reinterpret_cast<Reader*>(task->privdata);
    Local<Value> v(r->createString(str,len));

    if (task->type == REDIS_REPLY_ERROR)
        v = Exception::Error(v->ToString());
    return tryParentize(task,v);
}

static void *createInteger(const redisReadTask *task, long long value) {
    Nan::HandleScope scope;
    return tryParentize(task, Nan::New<Number>(value));
}

static void *createNil(const redisReadTask *task) {
    Nan::HandleScope scope;
    return tryParentize(task, Nan::Null());
}

static redisReplyObjectFunctions v8ReplyFunctions = {
    createString,
    createArray,
    createInteger,
    createNil,
    0 /* No free function: cleanup is done in Reader::Get. */
};


Reader::Reader(const Napi::CallbackInfo& info) : Napi::ObjectWrap<Database>(info) {
    this->return_buffers = false;
    this->reader = redisReaderCreateWithFunctions(&v8ReplyFunctions);
    this->eader->privdata = this;
    /*#if _USE_CUSTOM_BUFFER_POOL
    if (return_buffers) {
        Local<Object> global = Context::GetCurrent()->Global();
        Local<Value> bv = global->Get(String::NewSymbol("Buffer"));
        assert(bv->IsFunction());
        Local<Function> bf = Local<Function>::Cast(bv);
        buffer_fn = Persistent<Function>::New(bf);

        buffer_pool_length = 8*1024; // Same as node 
        buffer_pool_offset = 0;

        node::Buffer *b = node::Buffer::New(buffer_pool_length);
        buffer_pool = Persistent<Object>::New(b->handle_);
    }
#endif*/
  }

Reader::~Reader() {
    redisReaderFree(reader);
}

/* Don't declare an extra scope here, so the objects are created within the
 * scope inherited from the caller (Reader::Get) and we don't have to the pay
 * the overhead. */
inline Local<Value> Reader::createString(char *str, size_t len) {
    if (return_buffers) {
#if _USE_CUSTOM_BUFFER_POOL
        if (len > buffer_pool_length) {
            node::Buffer *b = node::Buffer::New(str,len);
            return Local<Value>::New(b->handle_);
        } else {
            return createBufferFromPool(str,len);
        }
#else
        return Nan::CopyBuffer(str,len).ToLocalChecked();
#endif
    } else {
        return Nan::New<String>(str,len).ToLocalChecked();
    }
}

/*#if _USE_CUSTOM_BUFFER_POOL
Local<Value> Reader::createBufferFromPool(char *str, size_t len) {
    HandleScope scope;
    Local<Value> argv[3];
    Local<Object> instance;

    assert(len <= buffer_pool_length);
    if (buffer_pool_length - buffer_pool_offset < len) {
        node::Buffer *b = node::Buffer::New(buffer_pool_length);
        buffer_pool.Dispose();
        buffer_pool = Persistent<Object>::New(b->handle_);
        buffer_pool_offset = 0;
    }

    memcpy(node::Buffer::Data(buffer_pool)+buffer_pool_offset,str,len);

    argv[0] = Local<Value>::New(buffer_pool);
    argv[1] = Integer::New(len);
    argv[2] = Integer::New(buffer_pool_offset);
    instance = buffer_fn->NewInstance(3,argv);
    buffer_pool_offset += len;
    return scope.Close(instance);
}
#endif*/

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


    Reader *r = Nan::ObjectWrap::Unwrap<Reader>(info.This());

    if (info.Length() == 0) {
        Nan::ThrowTypeError("First argument must be a string or buffer");
    } else {
        if (node::Buffer::HasInstance(info[0])) {
            Local<Object> buffer_object = info[0].As<Object>();
            char *data;
            size_t length;

            data = node::Buffer::Data(buffer_object);
            length = node::Buffer::Length(buffer_object);

            /* Can't handle OOM for now. */
            assert(redisReaderFeed(r->reader, data, length) == REDIS_OK);
        } else if (info[0]->IsString()) {
            Nan::Utf8String str(info[0].As<String>());
            redisReplyReaderFeed(r->reader, *str, str.length());
        } else {
            Nan::ThrowError("Invalid argument");
        }
    }

    info.GetReturnValue().Set(info.This());


}

Napi::Value Reader::Get(const Napi::CallbackInfo& info) {
    
    
    Reader *r = Nan::ObjectWrap::Unwrap<Reader>(info.This());
    void *index = NULL;
    Local<Value> reply;
    int i;

    if (redisReaderGetReply(r->reader,&index) == REDIS_OK) {
        if (index == 0) {
            return;
        } else {
            /* Complete replies should always have a root object at index 1. */
            assert((size_t)index == 1);
            reply = Nan::New(r->handle[1]);

            /* Dispose and clear used handles. */
            for (i = 1; i < 3; i++) {
                r->handle[i].Reset();
            }
        }
    } else {
        Nan::ThrowError(r->reader->errstr);
    }

    info.GetReturnValue().Set(reply);


}
