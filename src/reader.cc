#include <v8.h>
#include <node.h>
#include <node_buffer.h>
#include <string.h>
#include <assert.h>
#include "reader.h"

using namespace hiredis;

static void *tryParentize(const redisReadTask *task, const Local<Value> &v) {
    Reader *r = reinterpret_cast<Reader*>(task->privdata);
    size_t pidx, vidx;

    if (task->parent != NULL) {
        pidx = (size_t)task->parent->obj;
        assert(pidx > 0 && pidx < 9);

        /* When there is a parent, it should be an array. */
        Local<Value> lhandle = NanPersistentToLocal(r->handle[pidx]);
        assert(lhandle->IsArray());
        Local<Array> parent = lhandle.As<Array>();
        parent->Set(task->idx,v);

        /* Store the handle when this is an inner array. Otherwise, hiredis
         * doesn't care about the return value as long as the value is set in
         * its parent array. */
        vidx = pidx+1;
        if (v->IsArray()) {
            NanDispose(r->handle[vidx]);
            NanAssignPersistent(Value, r->handle[vidx], v);
            return (void*)vidx;
        } else {
            /* Return value doesn't matter for inner value, as long as it is
             * not NULL (which means OOM for hiredis). */
            return (void*)0xcafef00d;
        }
    } else {
        /* There is no parent, so this value is the root object. */
        NanAssignPersistent(Value, r->handle[1], v);
        return (void*)1;
    }
}

static void *createArray(const redisReadTask *task, int size) {
    Local<Value> v(Array::New(size));
    return tryParentize(task,v);
}

static void *createString(const redisReadTask *task, char *str, size_t len) {
    Reader *r = reinterpret_cast<Reader*>(task->privdata);
    Local<Value> v(r->createString(str,len));

    if (task->type == REDIS_REPLY_ERROR)
        v = Exception::Error(v->ToString());
    return tryParentize(task,v);
}

static void *createInteger(const redisReadTask *task, long long value) {
    Local<Value> v(Number::New(value));
    return tryParentize(task,v);
}

static void *createNil(const redisReadTask *task) {
    Local<Value> v(Local<Value>::New(Null()));
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

#if NODE_MODULE_VERSION < 0xC
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
#endif
}

Reader::~Reader() {
    redisReaderFree(reader);
}

/* Don't use a HandleScope here, so the objects are created within the scope of
 * the caller (Reader::Get) and we don't have to the pay the overhead. */
inline Local<Value> Reader::createString(char *str, size_t len) {
    if (return_buffers) {
#if NODE_MODULE_VERSION < 0xC
        if (len > buffer_pool_length) {
            Buffer *b = Buffer::New(str,len);
            return Local<Value>::New(b->handle_);
        } else {
            return createBufferFromPool(str,len);
        }
#else
        return NanNewBufferHandle(str,len);
#endif
    } else {
        return String::New(str,len);
    }
}

#if NODE_MODULE_VERSION < 0xC
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
#endif

NAN_METHOD(Reader::New) {
    NanScope();
    bool return_buffers = false;

    if (args.Length() > 0 && args[0]->IsObject()) {
        Local<Value> bv = args[0].As<Object>()->Get(String::New("return_buffers"));
        if (bv->IsBoolean())
            return_buffers = bv->ToBoolean()->Value();
    }

    Reader *r = new Reader(return_buffers);
    r->Wrap(args.This());
    NanReturnValue(args.This());
}

void Reader::Initialize(Handle<Object> target) {
    NanScope();

    Local<FunctionTemplate> t = FunctionTemplate::New(New);
    t->InstanceTemplate()->SetInternalFieldCount(1);
    NODE_SET_PROTOTYPE_METHOD(t, "feed", Feed);
    NODE_SET_PROTOTYPE_METHOD(t, "get", Get);
    target->Set(String::NewSymbol("Reader"), t->GetFunction());
}

NAN_METHOD(Reader::Feed) {
    NanScope();

    Reader *r = ObjectWrap::Unwrap<Reader>(args.This());
    if (args.Length() == 0) {
        NanThrowTypeError("First argument must be a string or buffer");
    } else {
        if(Buffer::HasInstance(args[0])) {
           Local<Object> buffer_object = args[0].As<Object>();
           char *data;
           size_t length;

           data = Buffer::Data(buffer_object);
           length = Buffer::Length(buffer_object);

           /* Can't handle OOM for now. */
           assert(redisReaderFeed(r->reader, data, length) == REDIS_OK);
       } else if (args[0]->IsString()) {
           String::Utf8Value str(args[0]->ToString());
           redisReplyReaderFeed(r->reader, *str, str.length());
       } else {
           NanThrowError("Invalid argument");
       }

    }

    NanReturnValue(args.This());
}

NAN_METHOD(Reader::Get) {
    NanScope();
    Reader *r = ObjectWrap::Unwrap<Reader>(args.This());
    void *index = NULL;
    Local<Value> reply;
    int i;

    if (redisReaderGetReply(r->reader,&index) == REDIS_OK) {
        if (index == 0) {
            NanReturnUndefined();
        } else {
            /* Complete replies should always have a root object at index 1. */
            assert((size_t)index == 1);
            reply = NanPersistentToLocal(r->handle[1]);

            /* Dispose and clear used handles. */
            for (i = 1; i < 3; i++) {
                NanDispose(r->handle[i]);
            }
        }
    } else {
        NanThrowError(r->reader->errstr);
    }
    NanReturnValue(reply);
}
