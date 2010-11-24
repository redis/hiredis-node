#include <v8.h>
#include <node.h>

namespace hiredis {

using namespace v8;
using namespace node;

class Reader : ObjectWrap {
public:
    Reader();
    static void Initialize(Handle<Object> target);
    static Handle<Value> New(const Arguments& args);
    static Handle<Value> Feed(const Arguments &args);
    static Handle<Value> Get(const Arguments &args);

private:
    void *reader;
};

};

