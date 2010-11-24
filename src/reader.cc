#include <v8.h>
#include <node.h>
#include <hiredis/hiredis.h>
#include "reader.h"

using namespace hiredis;

static inline Persistent<Value>* val_persist(const Local<Value> &v) {
  Persistent<Value> *val = new Persistent<Value>();
  *val = Persistent<Value>::New(v);
  return val;
}

static inline Persistent<Value>* val_unwrap(void *data) {
  return reinterpret_cast<Persistent<Value>*>(data);
}

static void *tryParentize(const redisReadTask *task, const Local<Value> &v) {
    if (task->parent != NULL) {
        Persistent<Value> *_p = val_unwrap(task->parent);
        assert((*_p)->IsArray());

        /* We know it is an array, so we can safely cast it */
        Local<Array> parent = Local<Array>::Cast((*_p)->ToObject());
        parent->Set(task->idx,v);
        return _p;
    }
    return val_persist(v);
}

static void *createString(const redisReadTask *task, char *str, size_t len) {
    return tryParentize(task,String::New(str,len));
}

static void *createArray(const redisReadTask *task, int size) {
    return tryParentize(task,Array::New(size));
}

static void *createInteger(const redisReadTask *task, long long value) {
    return tryParentize(task,Integer::New((int)value));
}

static void *createNil(const redisReadTask *task) {
    return tryParentize(task,Local<Value>::New(Null()));
}

static void freeObject(void *obj) {
    Persistent<Value> *value = val_unwrap(obj);
    value->Dispose();
    delete value;
}

static redisReplyObjectFunctions v8ReplyFunctions = {
    createString,
    createArray,
    createInteger,
    createNil,
    freeObject
};

Reader::Reader() {
    reader = redisReplyReaderCreate();
    redisReplyReaderSetReplyObjectFunctions(reader, &v8ReplyFunctions);
}

Handle<Value> Reader::New(const Arguments& args) {
    HandleScope scope;

    Reader *r = new Reader();
    r->Wrap(args.This());
    return args.This();
}

void Reader::Initialize(Handle<Object> target) {
    HandleScope scope;

    Local<FunctionTemplate> t = FunctionTemplate::New(New);
    t->InstanceTemplate()->SetInternalFieldCount(1);
    NODE_SET_PROTOTYPE_METHOD(t, "feed", Feed);
    NODE_SET_PROTOTYPE_METHOD(t, "get", Get);
    target->Set(String::NewSymbol("Reader"), t->GetFunction());
}

Handle<Value> Reader::Feed(const Arguments &args) {
    HandleScope scope;
    Reader *r = ObjectWrap::Unwrap<Reader>(args.This());

    if (args.Length() == 0 || !args[0]->IsString()) {
        return ThrowException(Exception::Error(
                    String::New("First argument must be a string")));
    }

    String::Utf8Value data(args[0]->ToString());
    redisReplyReaderFeed(r->reader, *data, data.length());
    return args.This();
}

Handle<Value> Reader::Get(const Arguments &args) {
    HandleScope scope;
    Reader *r = ObjectWrap::Unwrap<Reader>(args.This());
    void *_wrapped;
    Persistent<Value> *_reply;
    Local<Value> reply;

    if (redisReplyReaderGetReply(r->reader,&_wrapped) == REDIS_OK) {
        _reply = val_unwrap(_wrapped);
    } else {
        assert(NULL);
    }

    reply = Local<Value>::New(*_reply);
    _reply->Dispose();
    delete _reply;
    return reply;
}

