#include <v8.h>
#include <node.h>
#include <node_buffer.h>
#include <node_version.h>
#include <string.h>
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

static Persistent<Value> *tryParentize(const redisReadTask *task, const Local<Value> &v) {
    Persistent<Value> *_v;
    Reader *r = reinterpret_cast<Reader*>(task->privdata);

    if (task->parent != NULL) {
        Persistent<Value> *_p = val_unwrap(task->parent->obj);
        assert((*_p)->IsArray());

        /* We know it is an array, so we can safely cast it */
        Local<Array> parent = Local<Array>::Cast((*_p)->ToObject());
        parent->Set(task->idx,v);

        /* When this is the last element of a nested array, the persistent
         * handle to it should be removed because it is no longer needed. */
        if (task->idx == task->parent->elements-1) {
            r->popPersistentPointer();
        }

        return NULL;
    }

    _v = val_persist(v);
    r->pushPersistentPointer(_v);
    return _v;
}

static void *createArray(const redisReadTask *task, int size) {
    Local<Value> v(Array::New(size));
    Persistent<Value> *_v = tryParentize(task,v);
    Reader *r = reinterpret_cast<Reader*>(task->privdata);

    /* When this is a nested array, wrap child array in a persistent handle so
     * we can return it to hiredis so it can be passed as parent for the
     * elements that it contains. */
    if (_v == NULL && size > 0) {
        _v = val_persist(v);

        /* This will bail on nested multi bulk replies with depth > 1, but because
         * hiredis doesn't support such nesting this we'll never get here. */
        r->pushPersistentPointer(_v);
    }

    return _v;
}

static void *createString(const redisReadTask *task, char *str, size_t len) {
    Local<Value> v;
    Reader *r = reinterpret_cast<Reader*>(task->privdata);
    if (r->return_buffers) {
#if NODE_VERSION_AT_LEAST(0,3,0)
        Buffer *b = Buffer::New(str,len);
#else
        Buffer *b = Buffer::New(len);
        memcpy(b->data(),str,len);
#endif
        v = Local<Value>::New(b->handle_);
    } else {
        v = String::New(str,len);
    }

    if (task->type == REDIS_REPLY_ERROR)
        v = Exception::Error(v->ToString());
    return tryParentize(task,v);
}

static void *createInteger(const redisReadTask *task, long long value) {
    Local<Value> v(Integer::New(value));
    return tryParentize(task,v);
}

static void *createNil(const redisReadTask *task) {
    Local<Value> v(Local<Value>::New(Null()));
    return tryParentize(task,v);
}

static void freeObject(void *obj) {
    /* Handle disposing the object(s) in Reader::Get. */
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
    assert(redisReplyReaderSetReplyObjectFunctions(reader, &v8ReplyFunctions) == REDIS_OK);
    assert(redisReplyReaderSetPrivdata(reader, this) == REDIS_OK);

    pval[0] = pval[1] = NULL;
    pidx = 0;
    return_buffers = false;
}

Reader::~Reader() {
    redisReplyReaderFree(reader);
}

void Reader::pushPersistentPointer(Persistent<Value> *v) {
    assert(pidx < 2);
    pval[pidx++] = v;
}

void Reader::popPersistentPointer(void) {
    /* Don't pop the root object (at index 0) because it is the reply and
     * it needs to be tested for equality in Reader::Get. */
    if (pidx > 1) {
        pval[--pidx]->Dispose();
        delete pval[pidx];
        pval[pidx] = NULL;
    }
}

void Reader::clearPersistentPointers(void) {
    while(pidx > 0) {
        pval[--pidx]->Dispose();
        delete pval[pidx];
        pval[pidx] = NULL;
    }
}

Handle<Value> Reader::New(const Arguments& args) {
    HandleScope scope;

    Reader *r = new Reader();
    if (args.Length() > 0 && args[0]->IsObject()) {
        Local<Value> buffers = args[0]->ToObject()->Get(String::New("return_buffers"));
        if (buffers->IsBoolean()) {
            r->return_buffers = buffers->ToBoolean()->Value();
        }
    }

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

    if (args.Length() == 0) {
        return ThrowException(Exception::Error(
            String::New("First argument must be a string or buffer")));
    } else {
        if (Buffer::HasInstance(args[0])) {
            Local<Object> buffer_object = args[0]->ToObject();
            char *data;
            size_t length;

#if NODE_VERSION_AT_LEAST(0,3,0)
            data = Buffer::Data(buffer_object);
            length = Buffer::Length(buffer_object);
#else
            Buffer *buffer = ObjectWrap::Unwrap<Buffer>(buffer_object);
            data = buffer->data();
            length = buffer->length();
#endif

            redisReplyReaderFeed(r->reader, data, length);
        } else if (args[0]->IsString()) {
            String::Utf8Value str(args[0]->ToString());
            redisReplyReaderFeed(r->reader, *str, str.length());
        } else {
            return ThrowException(Exception::Error(
                String::New("Invalid argument")));
        }
    }

    return args.This();
}

Handle<Value> Reader::Get(const Arguments &args) {
    HandleScope scope;
    Reader *r = ObjectWrap::Unwrap<Reader>(args.This());
    void *_wrapped;
    Persistent<Value> *_reply;
    Local<Value> reply;

    if (redisReplyReaderGetReply(r->reader,&_wrapped) == REDIS_OK) {
        if (_wrapped == NULL) {
            /* Needs more data */
            return Undefined();
        } else {
            assert(_wrapped == r->pval[0]);
            _reply = val_unwrap(_wrapped);
        }
    } else {
        r->clearPersistentPointers();
        char *error = redisReplyReaderGetError(r->reader);
        return ThrowException(Exception::Error(String::New(error)));
    }

    reply = Local<Value>::New(*_reply);
    r->clearPersistentPointers();
    return reply;
}

