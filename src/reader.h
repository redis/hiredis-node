#include <nan.h>
#include <hiredis/read.h>

#if NODE_MODULE_VERSION < NODE_0_12_MODULE_VERSION
#define _USE_CUSTOM_BUFFER_POOL 1
#else
#define _USE_CUSTOM_BUFFER_POOL 0
#endif

namespace hiredis {

using namespace v8;

class Reader : public Nan::ObjectWrap {
public:
    Reader(bool);
    ~Reader();

    static NAN_MODULE_INIT(Initialize);
    static NAN_METHOD(New);
    static NAN_METHOD(Feed);
    static NAN_METHOD(Get);

    /* Objects created by the reply object functions need to get back to the
     * reader when the reply is requested via Reader::Get(). Keep temporary
     * objects in this handle. Use an array of handles because replies may
     * include nested multi bulks and child-elements need to be added to the
     * right respective parent. handle[0] will be unused, so the real index of
     * an object in this array can be returned from the reply object functions.
     * The returned value needs to be non-zero to distinguish complete replies
     * from incomplete replies. These are persistent handles because
     * Reader::Get might not return a full reply and the objects need to be
     * kept around for subsequent calls. */
    Nan::Persistent<Value> handle[9];

    /* Helper function to create string/buffer objects. */
    Local<Value> createString(char *str, size_t len);

private:
    redisReader *reader;

    /* Determines whether to return strings or buffers for single line and bulk
     * replies. This defaults to false, so strings are returned by default. */
    bool return_buffers;

#if _USE_CUSTOM_BUFFER_POOL
    Local<Value> createBufferFromPool(char *str, size_t len);
    Persistent<Function> buffer_fn;
    Persistent<Object> buffer_pool;
    size_t buffer_pool_length;
    size_t buffer_pool_offset;
#endif
};

};

