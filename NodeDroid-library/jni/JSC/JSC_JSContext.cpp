//
// Created by Eric on 11/6/16.
//

#include "JSC.h"

OpaqueJSContext::OpaqueJSContext(JSContext *ctx) : m_context(ctx)
{

}
OpaqueJSContext::~OpaqueJSContext()
{
    V8_ISOLATE(m_context->Group(), isolate);
        __android_log_print(ANDROID_LOG_DEBUG, "~OpaqueJSContext", "About to destructorize");
        m_context->SetDefunct();
        while(!isolate->IdleNotificationDeadline(
            group_->Platform()->MonotonicallyIncreasingTime() + 1.0)) {};
        __android_log_print(ANDROID_LOG_DEBUG, "~OpaqueJSContext", "Finished");
        int count = m_context->release();
        __android_log_print(ANDROID_LOG_DEBUG, "~OpaqueJSContext", "count = %d", count);
    V8_UNLOCK();

}

JS_EXPORT JSContextGroupRef JSContextGroupCreate()
{
    const ContextGroup *group = new ContextGroup();
    return group;
}

JS_EXPORT JSContextGroupRef JSContextGroupRetain(JSContextGroupRef group)
{
    ((ContextGroup*)group)->retain();
    return group;
}

JS_EXPORT void JSContextGroupRelease(JSContextGroupRef group)
{
    ((ContextGroup*)group)->release();
}

static JSContextGroupRef globalContextGroup = nullptr;

class GlobalContextGroup : public ContextGroup
{
    public:
        GlobalContextGroup() : ContextGroup() {}
        virtual ~GlobalContextGroup() {
            __android_log_print(ANDROID_LOG_DEBUG, "~GlobalContextGroup()", "destructing");
            globalContextGroup = nullptr;
        }
};

JS_EXPORT JSGlobalContextRef JSGlobalContextCreate(JSClassRef globalObjectClass)
{
    JSContextRef ctx = JSGlobalContextCreateInGroup(globalContextGroup, globalObjectClass);

    return (JSGlobalContextRef)ctx;
}

JS_EXPORT JSGlobalContextRef JSGlobalContextCreateInGroup(JSContextGroupRef group,
    JSClassRef globalObjectClass)
{
    JSGlobalContextRef ctx;
    bool created = false;

    if (!group) {
        if (!globalContextGroup) {
            globalContextGroup = new GlobalContextGroup();
            created = true;
        }
        group = globalContextGroup;
    }

    {
        V8_ISOLATE((ContextGroup*)group,isolate)
            if (globalObjectClass) {
                Local<Object> data;
                Local<ObjectTemplate> templ = globalObjectClass->NewTemplate(&data);
                Local<Context> context = Context::New(isolate, nullptr, templ);
                {
                    Context::Scope context_scope_(context);
                    Local<Object> global =
                        context->Global()->GetPrototype()->ToObject(context).ToLocalChecked();
                    ctx = new OpaqueJSContext(new JSContext((ContextGroup*)group, context));
                    TempJSValue value(globalObjectClass->InitInstance(ctx, global, data));
                    Local<Private> privateKey = v8::Private::ForApi(isolate,
                        String::NewFromUtf8(isolate, "__private"));
                    global->SetPrivate(context, privateKey,
                        Number::New(isolate,(double)reinterpret_cast<long>(nullptr)));
                }
            } else {
                ctx = new OpaqueJSContext(
                    new JSContext((ContextGroup*)group, Context::New(isolate)));
            }

            // Apparently JavaScriptCore implements console.log out of the box.  V8 doesn't.
            Local<Context> context = ctx->Context()->Value();
            Context::Scope context_scope_(context);
            Local<Object> global =
                context->Global()->GetPrototype()->ToObject(context).ToLocalChecked();
            Local<Object> console = Object::New(isolate);
            Local<Object> Symbol =
                context->Global()->Get(String::NewFromUtf8(isolate, "Symbol"))->ToObject();
            Local<Value> toStringTag = Symbol->Get(String::NewFromUtf8(isolate, "toStringTag"));
            Local<Object> consolePrototype = Object::New(isolate);
            consolePrototype->Set(context, toStringTag, String::NewFromUtf8(isolate, "Console"));
            console->SetPrototype(context, consolePrototype);
            global->DefineOwnProperty(context, String::NewFromUtf8(isolate, "console"), console,
                v8::DontEnum);
            Local<FunctionTemplate> logt = FunctionTemplate::New(isolate,
                [](const FunctionCallbackInfo< Value > &info) {
                    Isolate::Scope isolate_scope_(info.GetIsolate());
                    HandleScope handle_scope_(info.GetIsolate());

                    String::Utf8Value str(info[0]->ToString(info.GetIsolate()));
                    __android_log_print(ANDROID_LOG_INFO, "[JSC] console.log", "%s", *str);
                }
            );
            console->Set(context, String::NewFromUtf8(isolate, "log"),
                logt->GetFunction(context).ToLocalChecked());

        V8_UNLOCK()
    }

    if (created)
        ((ContextGroup*)group)->release();

    return ctx;
}

JS_EXPORT JSGlobalContextRef JSGlobalContextRetain(JSGlobalContextRef ctx)
{
    ctx->retain();
    return ctx;
}

JS_EXPORT void JSGlobalContextRelease(JSGlobalContextRef ctx)
{
    ctx->release();
}

JS_EXPORT JSObjectRef JSContextGetGlobalObject(JSContextRef ctx)
{
    JSObjectRef v;

    V8_ISOLATE_CTX(ctx->Context(),isolate,Ctx)
        JSValue<Object> *object = context_->Global();
        Local<Object> global = object->Value();
        v = new OpaqueJSValue(ctx, global);
        object->release();
    V8_UNLOCK()

    return v;
}

JS_EXPORT JSContextGroupRef JSContextGetGroup(JSContextRef ctx)
{
    ctx->Context()->Group()->retain();
    return ctx->Context()->Group();
}

JS_EXPORT JSGlobalContextRef JSContextGetGlobalContext(JSContextRef ctx)
{
    // FIXME: What is this supposed to do?
    // FIXME: Gets the global context of a JavaScript execution context.
    ctx->Context()->retain();
    return (JSGlobalContextRef)ctx;
}

JS_EXPORT JSStringRef JSGlobalContextCopyName(JSGlobalContextRef ctx)
{
    JSStringRef n = nullptr;
    V8_ISOLATE_CTX(ctx->Context(),isolate,context)
        Local<Value> name = context->GetEmbedderData(0);
        if (!name->IsUndefined()) {
            String::Utf8Value str(name->ToString(context).ToLocalChecked());
            n = JSStringCreateWithUTF8CString(*str);
        }
    V8_UNLOCK()
    return n;
}

JS_EXPORT void JSGlobalContextSetName(JSGlobalContextRef ctx, JSStringRef name)
{
    V8_ISOLATE(ctx->Context()->Group(), isolate)
        if (name) {
            ctx->Context()->Value()->SetEmbedderData(0, name->Value(isolate));
        } else {
            ctx->Context()->Value()->
                SetEmbedderData(0, Local<Value>::New(isolate,Undefined(isolate)));
        }
    V8_UNLOCK()
}