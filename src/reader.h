#include <napi.h>
#include <hiredis/read.h>

namespace hiredis {

    class Reader : public Napi::ObjectWrap<Reader> {
    public:
        explicit Reader(const Napi::CallbackInfo& info);
        ~Reader();

        static Napi::Object Initialize(Napi::Env env, Napi::Object exports);
        void Feed(const Napi::CallbackInfo& info);
        Napi::Value Get(const Napi::CallbackInfo& info);
        /* Helper function to create string/buffer objects. */
        Napi::Value createString(char *str, size_t len);
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
        Napi::Reference<Napi::Value> handle[9];
        
    private:
        redisReader *reader;

        /* Determines whether to return strings or buffers for single line and bulk
        * replies. This defaults to false, so strings are returned by default. */
        bool return_buffers;

        static Napi::FunctionReference constructor;
    };

};

