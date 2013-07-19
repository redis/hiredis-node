#include <v8.h>
#include <node.h>
#include <node_buffer.h>
#include <string.h>
#include <assert.h>
#include "reader.h"

using namespace hiredis;

static void *tryParentize(const redisReadTask *task, const Local<Value> &v) {
#if NODE_VERSION_AT_LEAST(0, 11, 4)
    Isolate *isolate = Isolate::GetCurrent();
//    HandleScope scope(isolate);

#endif /* NODE_VERSION_AT_LEAST(0, 11, 4) */
    Reader *r = reinterpret_cast<Reader*>(task->privdata);
    size_t pidx, vidx;

    if (task->parent != NULL) {
        pidx = (size_t)task->parent->obj;
        assert(pidx > 0 && pidx < 9);

        /* When there is a parent, it should be an array. */
#if !NODE_VERSION_AT_LEAST(0, 11, 4)
        assert(r->handle[pidx]->IsArray());
        Local<Array> parent = Local<Array>::Cast(r->handle[pidx]->ToObject());
#else /* NODE_VERSION_AT_LEAST(0, 11, 4) */
        Local<Value> lhandle = Local<Value>::New(isolate, r->handle[pidx]);
        assert(lhandle->IsArray());
        Local<Array> parent = Local<Array>::Cast(lhandle->ToObject());
#endif /* NODE_VERSION_AT_LEAST(0, 11, 4) */
        parent->Set(task->idx,v);

        /* Store the handle when this is an inner array. Otherwise, hiredis
         * doesn't care about the return value as long as the value is set in
         * its parent array. */
        vidx = pidx+1;
        if (v->IsArray()) {
#if !NODE_VERSION_AT_LEAST(0, 11, 4)
            r->handle[vidx].Dispose();
            r->handle[vidx].Clear();
            r->handle[vidx] = Persistent<Value>::New(v);
#else /* NODE_VERSION_AT_LEAST(0, 11, 4) */
            r->handle[vidx].Reset(isolate, v);
//            return scope.Close((void*)vidx);
#endif /* NODE_VERSION_AT_LEAST(0, 11, 4) */
            return (void*)vidx;
        } else {
            /* Return value doesn't matter for inner value, as long as it is
             * not NULL (which means OOM for hiredis). */
#if NODE_VERSION_AT_LEAST(0, 11, 4)
//            return scope.Close((void*)0xcafef00d);
#endif /* NODE_VERSION_AT_LEAST(0, 11, 4) */
            return (void*)0xcafef00d;
        }
    } else {
        /* There is no parent, so this value is the root object. */
#if !NODE_VERSION_AT_LEAST(0, 11, 4)
        r->handle[1] = Persistent<Value>::New(v);
#else /* NODE_VERSION_AT_LEAST(0, 11, 4) */
        r->handle[1].Reset(isolate, v);
//        return scope.Close((void*)1);
#endif /* NODE_VERSION_AT_LEAST(0, 11, 4) */
        return (void*)1;
    }
}

static void *createArray(const redisReadTask *task, int size) {
#if NODE_VERSION_AT_LEAST(0, 11, 4)
//    Isolate *isolate = Isolate::GetCurrent();
//    HandleScope scope(isolate);

#endif /* NODE_VERSION_AT_LEAST(0, 11, 4) */
    Local<Value> v(Array::New(size));
#if NODE_VERSION_AT_LEAST(0, 11, 4)
//    return scope.Close(tryParentize(task,v));
#endif /* NODE_VERSION_AT_LEAST(0, 11, 4) */
    return tryParentize(task,v);
}

static void *createString(const redisReadTask *task, char *str, size_t len) {
#if NODE_VERSION_AT_LEAST(0, 11, 4)
//    Isolate *isolate = Isolate::GetCurrent();
//    HandleScope scope(isolate);

#endif /* NODE_VERSION_AT_LEAST(0, 11, 4) */
    Reader *r = reinterpret_cast<Reader*>(task->privdata);
    Local<Value> v(r->createString(str,len));

    if (task->type == REDIS_REPLY_ERROR)
        v = Exception::Error(v->ToString());
#if NODE_VERSION_AT_LEAST(0, 11, 4)
//    return scope.Close(tryParentize(task,v));
#endif /* NODE_VERSION_AT_LEAST(0, 11, 4) */
    return tryParentize(task,v);
}

static void *createInteger(const redisReadTask *task, long long value) {
#if NODE_VERSION_AT_LEAST(0, 11, 4)
//    Isolate *isolate = Isolate::GetCurrent();
//    HandleScope scope(isolate);

#endif /* NODE_VERSION_AT_LEAST(0, 11, 4) */
    Local<Value> v(Number::New(value));
#if NODE_VERSION_AT_LEAST(0, 11, 4)
//    return scope.Close(tryParentize(task,v));
#endif /* NODE_VERSION_AT_LEAST(0, 11, 4) */
    return tryParentize(task,v);
}

static void *createNil(const redisReadTask *task) {
#if NODE_VERSION_AT_LEAST(0, 11, 4)
//    Isolate *isolate = Isolate::GetCurrent();
//    HandleScope scope(isolate);

#endif /* NODE_VERSION_AT_LEAST(0, 11, 4) */
    Local<Value> v(Local<Value>::New(Null()));
#if NODE_VERSION_AT_LEAST(0, 11, 4)
//    return scope.Close(tryParentize(task,v));
#endif /* NODE_VERSION_AT_LEAST(0, 11, 4) */
    return tryParentize(task,v);
}

#if NODE_VERSION_AT_LEAST(0, 11, 4)

#endif /* NODE_VERSION_AT_LEAST(0, 11, 4) */
static redisReplyObjectFunctions v8ReplyFunctions = {
    createString,
    createArray,
    createInteger,
    createNil,
    NULL /* No free function: cleanup is done in Reader::Get. */
};

Reader::Reader(bool return_buffers) :
    return_buffers(return_buffers)
{
    reader = redisReaderCreate();
    reader->fn = &v8ReplyFunctions;
    reader->privdata = this;
#if !NODE_VERSION_AT_LEAST(0, 11, 4)

    if (return_buffers) {
        Local<Object> global = Context::GetCurrent()->Global();
        Local<Value> bv = global->Get(String::NewSymbol("Buffer"));
        assert(bv->IsFunction());
        Local<Function> bf = Local<Function>::Cast(bv);
        buffer_fn = Persistent<Function>::New(bf);

        buffer_pool_length = 8*1024; /* Same as node */
        buffer_pool_offset = 0;

        Buffer *b = Buffer::New(buffer_pool_length);
        buffer_pool = Persistent<Object>::New(b->handle_);
    }
#endif /* ! NODE_VERSION_AT_LEAST(0, 11, 4) */
}

Reader::~Reader() {
    redisReaderFree(reader);
}

/* Don't use a HandleScope here, so the objects are created within the scope of
 * the caller (Reader::Get) and we don't have to the pay the overhead. */
#if !NODE_VERSION_AT_LEAST(0, 11, 4)
Local<Value> Reader::createString(char *str, size_t len) {
#else /* NODE_VERSION_AT_LEAST(0, 11, 4) */
inline Local<Value> Reader::createString(char *str, size_t len) {
#endif /* NODE_VERSION_AT_LEAST(0, 11, 4) */
    if (return_buffers) {
#if !NODE_VERSION_AT_LEAST(0, 11, 4)
        if (len > buffer_pool_length) {
            Buffer *b = Buffer::New(str,len);
            return Local<Value>::New(b->handle_);
        } else {
            return createBufferFromPool(str,len);
        }
#else /* NODE_VERSION_AT_LEAST(0, 11, 4) */
            Local<Object> b = Buffer::New(str,len);
            return Local<Value>::New(b);
#endif /* NODE_VERSION_AT_LEAST(0, 11, 4) */
    } else {
        return String::New(str,len);
    }
}

#if !NODE_VERSION_AT_LEAST(0, 11, 4)
Local<Value> Reader::createBufferFromPool(char *str, size_t len) {
    HandleScope scope;
    Local<Value> argv[3];
    Local<Object> instance;

    assert(len <= buffer_pool_length);
    if (buffer_pool_length - buffer_pool_offset < len) {
        Buffer *b = Buffer::New(buffer_pool_length);
        buffer_pool.Dispose();
        buffer_pool = Persistent<Object>::New(b->handle_);
        buffer_pool_offset = 0;
    }

    memcpy(Buffer::Data(buffer_pool)+buffer_pool_offset,str,len);

    argv[0] = Local<Value>::New(buffer_pool);
    argv[1] = Integer::New(len);
    argv[2] = Integer::New(buffer_pool_offset);
    instance = buffer_fn->NewInstance(3,argv);
    buffer_pool_offset += len;
    return scope.Close(instance);
}

Handle<Value> Reader::New(const Arguments& args) {
    HandleScope scope;
#else /* NODE_VERSION_AT_LEAST(0, 11, 4) */
template<class T> void Reader::New(const v8::FunctionCallbackInfo<T> & info) {
    Isolate *isolate = Isolate::GetCurrent();
    HandleScope scope(isolate);
#endif /* NODE_VERSION_AT_LEAST(0, 11, 4) */
    bool return_buffers = false;

#if !NODE_VERSION_AT_LEAST(0, 11, 4)
    if (args.Length() > 0 && args[0]->IsObject()) {
        Local<Value> bv = args[0]->ToObject()->Get(String::New("return_buffers"));
#else /* NODE_VERSION_AT_LEAST(0, 11, 4) */
    if (info.Length() > 0 && info[0]->IsObject()) {
        Local<Value> bv = info[0]->ToObject()->Get(String::New("return_buffers"));
#endif /* NODE_VERSION_AT_LEAST(0, 11, 4) */
        if (bv->IsBoolean())
            return_buffers = bv->ToBoolean()->Value();
    }

    Reader *r = new Reader(return_buffers);
#if !NODE_VERSION_AT_LEAST(0, 11, 4)
    r->Wrap(args.This());
    return scope.Close(args.This());
#else /* NODE_VERSION_AT_LEAST(0, 11, 4) */
    r->Wrap(info.This());
    info.GetReturnValue().Set(info.This());
#endif /* NODE_VERSION_AT_LEAST(0, 11, 4) */
}

void Reader::Initialize(Handle<Object> target) {
#if !NODE_VERSION_AT_LEAST(0, 11, 4)
    HandleScope scope;
#else /* NODE_VERSION_AT_LEAST(0, 11, 4) */
    Isolate *isolate = Isolate::GetCurrent();
    HandleScope scope(isolate);
#endif /* NODE_VERSION_AT_LEAST(0, 11, 4) */

    Local<FunctionTemplate> t = FunctionTemplate::New(New);
    t->InstanceTemplate()->SetInternalFieldCount(1);
    NODE_SET_PROTOTYPE_METHOD(t, "feed", Feed);
    NODE_SET_PROTOTYPE_METHOD(t, "get", Get);
    target->Set(String::NewSymbol("Reader"), t->GetFunction());
}

#if !NODE_VERSION_AT_LEAST(0, 11, 4)
Handle<Value> Reader::Feed(const Arguments &args) {
    HandleScope scope;
    Reader *r = ObjectWrap::Unwrap<Reader>(args.This());

    if (args.Length() == 0) {
        return ThrowException(Exception::Error(
            String::New("First argument must be a string or buffer")));
#else /* NODE_VERSION_AT_LEAST(0, 11, 4) */
template<class T> void  Reader::Feed(const v8::FunctionCallbackInfo<T> &info) {
    Isolate *isolate = Isolate::GetCurrent();
    HandleScope scope(isolate);
    Reader *r = ObjectWrap::Unwrap<Reader>(info.This());

    if (info.Length() == 0) {
        info.GetReturnValue().Set(ThrowException(Exception::Error(
            String::New("First argument must be a string or buffer"))));
        return;
#endif /* NODE_VERSION_AT_LEAST(0, 11, 4) */
    } else {
#if !NODE_VERSION_AT_LEAST(0, 11, 4)
        if (Buffer::HasInstance(args[0])) {
            Local<Object> buffer_object = args[0]->ToObject();
#else /* NODE_VERSION_AT_LEAST(0, 11, 4) */
        if (Buffer::HasInstance(info[0])) {
            Local<Object> buffer_object = info[0]->ToObject();
#endif /* NODE_VERSION_AT_LEAST(0, 11, 4) */
            char *data;
            size_t length;

            data = Buffer::Data(buffer_object);
            length = Buffer::Length(buffer_object);

            /* Can't handle OOM for now. */
            assert(redisReaderFeed(r->reader, data, length) == REDIS_OK);
#if !NODE_VERSION_AT_LEAST(0, 11, 4)
        } else if (args[0]->IsString()) {
            String::Utf8Value str(args[0]->ToString());
#else /* NODE_VERSION_AT_LEAST(0, 11, 4) */
        } else if (info[0]->IsString()) {
            String::Utf8Value str(info[0]->ToString());
#endif /* NODE_VERSION_AT_LEAST(0, 11, 4) */
            redisReplyReaderFeed(r->reader, *str, str.length());
        } else {
#if !NODE_VERSION_AT_LEAST(0, 11, 4)
            return ThrowException(Exception::Error(
                String::New("Invalid argument")));
#else /* NODE_VERSION_AT_LEAST(0, 11, 4) */
            info.GetReturnValue().Set(ThrowException(Exception::Error(
                String::New("Invalid argument"))));
            return;
#endif /* NODE_VERSION_AT_LEAST(0, 11, 4) */
        }
    }

#if !NODE_VERSION_AT_LEAST(0, 11, 4)
    return args.This();
#else /* NODE_VERSION_AT_LEAST(0, 11, 4) */
    info.GetReturnValue().Set(info.This());
#endif /* NODE_VERSION_AT_LEAST(0, 11, 4) */
}

#if !NODE_VERSION_AT_LEAST(0, 11, 4)
Handle<Value> Reader::Get(const Arguments &args) {
    HandleScope scope;
    Reader *r = ObjectWrap::Unwrap<Reader>(args.This());
#else /* NODE_VERSION_AT_LEAST(0, 11, 4) */
template<class T> void  Reader::Get(const v8::FunctionCallbackInfo<T> &info) {
    Isolate *isolate = Isolate::GetCurrent();
    HandleScope scope(isolate);
    Reader *r = ObjectWrap::Unwrap<Reader>(info.This());
#endif /* NODE_VERSION_AT_LEAST(0, 11, 4) */
    void *index = NULL;
    Local<Value> reply;
    int i;

    if (redisReaderGetReply(r->reader,&index) == REDIS_OK) {
        if (index == 0) {
#if !NODE_VERSION_AT_LEAST(0, 11, 4)
            return Undefined();
#else /* NODE_VERSION_AT_LEAST(0, 11, 4) */
            info.GetReturnValue().SetUndefined();
            return;
#endif /* NODE_VERSION_AT_LEAST(0, 11, 4) */
        } else {
            /* Complete replies should always have a root object at index 1. */
            assert((size_t)index == 1);
#if !NODE_VERSION_AT_LEAST(0, 11, 4)
            reply = Local<Value>::New(r->handle[1]);
#else /* NODE_VERSION_AT_LEAST(0, 11, 4) */
            reply = Local<Value>::New(Local<Value>::New(isolate, r->handle[1]));
#endif /* NODE_VERSION_AT_LEAST(0, 11, 4) */

            /* Dispose and clear used handles. */
            for (i = 1; i < 3; i++) {
                r->handle[i].Dispose();
                r->handle[i].Clear();
            }
        }
    } else {
#if !NODE_VERSION_AT_LEAST(0, 11, 4)
        return ThrowException(Exception::Error(String::New(r->reader->errstr)));
#else /* NODE_VERSION_AT_LEAST(0, 11, 4) */
        info.GetReturnValue().Set(ThrowException(Exception::Error(String::New(r->reader->errstr))));
        return;
#endif /* NODE_VERSION_AT_LEAST(0, 11, 4) */
    }

#if !NODE_VERSION_AT_LEAST(0, 11, 4)
    return scope.Close(reply);
#else /* NODE_VERSION_AT_LEAST(0, 11, 4) */
    info.GetReturnValue().Set(reply);
#endif /* NODE_VERSION_AT_LEAST(0, 11, 4) */
}
