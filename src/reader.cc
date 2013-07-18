#include <v8.h>
#include <node.h>
#include <node_buffer.h>
#include <string.h>
#include <assert.h>
#include "reader.h"

using namespace hiredis;

static void *tryParentize(const redisReadTask *task, const Local<Value> &v) {
    Isolate *isolate = Isolate::GetCurrent();
//    HandleScope scope(isolate);

    Reader *r = reinterpret_cast<Reader*>(task->privdata);
    size_t pidx, vidx;

    if (task->parent != NULL) {
        pidx = (size_t)task->parent->obj;
        assert(pidx > 0 && pidx < 9);

        /* When there is a parent, it should be an array. */
        Local<Value> lhandle = Local<Value>::New(isolate, r->handle[pidx]);
        assert(lhandle->IsArray());
        Local<Array> parent = Local<Array>::Cast(lhandle->ToObject());
        parent->Set(task->idx,v);

        /* Store the handle when this is an inner array. Otherwise, hiredis
         * doesn't care about the return value as long as the value is set in
         * its parent array. */
        vidx = pidx+1;
        if (v->IsArray()) {
            r->handle[vidx].Reset(isolate, v);
//            return scope.Close((void*)vidx);
            return (void*)vidx;
        } else {
            /* Return value doesn't matter for inner value, as long as it is
             * not NULL (which means OOM for hiredis). */
//            return scope.Close((void*)0xcafef00d);
            return (void*)0xcafef00d;
        }
    } else {
        /* There is no parent, so this value is the root object. */
        r->handle[1].Reset(isolate, v);
//        return scope.Close((void*)1);
        return (void*)1;
    }
}

static void *createArray(const redisReadTask *task, int size) {
//    Isolate *isolate = Isolate::GetCurrent();
//    HandleScope scope(isolate);

    Local<Value> v(Array::New(size));
//    return scope.Close(tryParentize(task,v));
    return tryParentize(task,v);
}

static void *createString(const redisReadTask *task, char *str, size_t len) {
//    Isolate *isolate = Isolate::GetCurrent();
//    HandleScope scope(isolate);

    Reader *r = reinterpret_cast<Reader*>(task->privdata);
    Local<Value> v(r->createString(str,len));

    if (task->type == REDIS_REPLY_ERROR)
        v = Exception::Error(v->ToString());
//    return scope.Close(tryParentize(task,v));
    return tryParentize(task,v);
}

static void *createInteger(const redisReadTask *task, long long value) {
//    Isolate *isolate = Isolate::GetCurrent();
//    HandleScope scope(isolate);

    Local<Value> v(Number::New(value));
//    return scope.Close(tryParentize(task,v));
    return tryParentize(task,v);
}

static void *createNil(const redisReadTask *task) {
//    Isolate *isolate = Isolate::GetCurrent();
//    HandleScope scope(isolate);

    Local<Value> v(Local<Value>::New(Null()));
//    return scope.Close(tryParentize(task,v));
    return tryParentize(task,v);
}


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
}

Reader::~Reader() {
    redisReaderFree(reader);
}

/* Don't use a HandleScope here, so the objects are created within the scope of
 * the caller (Reader::Get) and we don't have to the pay the overhead. */
inline Local<Value> Reader::createString(char *str, size_t len) {
    if (return_buffers) {
            Local<Object> b = Buffer::New(str,len);
            return Local<Value>::New(b);
    } else {
        return String::New(str,len);
    }
}

template<class T> void Reader::New(const v8::FunctionCallbackInfo<T> & info) {
    Isolate *isolate = Isolate::GetCurrent();
    HandleScope scope(isolate);
    bool return_buffers = false;

    if (info.Length() > 0 && info[0]->IsObject()) {
        Local<Value> bv = info[0]->ToObject()->Get(String::New("return_buffers"));
        if (bv->IsBoolean())
            return_buffers = bv->ToBoolean()->Value();
    }

    Reader *r = new Reader(return_buffers);
    r->Wrap(info.This());
    info.GetReturnValue().Set(info.This());
}

void Reader::Initialize(Handle<Object> target) {
    Isolate *isolate = Isolate::GetCurrent();
    HandleScope scope(isolate);

    Local<FunctionTemplate> t = FunctionTemplate::New(New);
    t->InstanceTemplate()->SetInternalFieldCount(1);
    NODE_SET_PROTOTYPE_METHOD(t, "feed", Feed);
    NODE_SET_PROTOTYPE_METHOD(t, "get", Get);
    target->Set(String::NewSymbol("Reader"), t->GetFunction());
}

template<class T> void  Reader::Feed(const v8::FunctionCallbackInfo<T> &info) {
    Isolate *isolate = Isolate::GetCurrent();
    HandleScope scope(isolate);
    Reader *r = ObjectWrap::Unwrap<Reader>(info.This());

    if (info.Length() == 0) {
        info.GetReturnValue().Set(ThrowException(Exception::Error(
            String::New("First argument must be a string or buffer"))));
        return;
    } else {
        if (Buffer::HasInstance(info[0])) {
            Local<Object> buffer_object = info[0]->ToObject();
            char *data;
            size_t length;

            data = Buffer::Data(buffer_object);
            length = Buffer::Length(buffer_object);

            /* Can't handle OOM for now. */
            assert(redisReaderFeed(r->reader, data, length) == REDIS_OK);
        } else if (info[0]->IsString()) {
            String::Utf8Value str(info[0]->ToString());
            redisReplyReaderFeed(r->reader, *str, str.length());
        } else {
            info.GetReturnValue().Set(ThrowException(Exception::Error(
                String::New("Invalid argument"))));
            return;
        }
    }

    info.GetReturnValue().Set(info.This());
}

template<class T> void  Reader::Get(const v8::FunctionCallbackInfo<T> &info) {
    Isolate *isolate = Isolate::GetCurrent();
    HandleScope scope(isolate);
    Reader *r = ObjectWrap::Unwrap<Reader>(info.This());
    void *index = NULL;
    Local<Value> reply;
    int i;

    if (redisReaderGetReply(r->reader,&index) == REDIS_OK) {
        if (index == 0) {
            info.GetReturnValue().SetUndefined();
            return;
        } else {
            /* Complete replies should always have a root object at index 1. */
            assert((size_t)index == 1);
            reply = Local<Value>::New(Local<Value>::New(isolate, r->handle[1]));

            /* Dispose and clear used handles. */
            for (i = 1; i < 3; i++) {
                r->handle[i].Dispose();
                r->handle[i].Clear();
            }
        }
    } else {
        info.GetReturnValue().Set(ThrowException(Exception::Error(String::New(r->reader->errstr))));
        return;
    }

    info.GetReturnValue().Set(reply);
}
