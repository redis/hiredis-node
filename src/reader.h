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

    void setNestedArrayPointer(Persistent<Value>*);
    void clearNestedArrayPointer();

private:
    void *reader;

    /* For nested multi bulks, we need to emit persistent value pointers for
     * nested arrays, that can be passed as parent for underlying elements.
     * These pointers need to be cleaned up when they are no longer needed, so
     * keep a reference here. */
    Persistent<Value>* pval;

};

};

