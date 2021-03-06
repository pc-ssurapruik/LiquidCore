//
// JSC_JSObject.cpp
//
// LiquidPlayer project
// https://github.com/LiquidPlayer
//
// Created by Eric Lange
//
/*
 Copyright (c) 2016 Eric Lange. All rights reserved.

 Redistribution and use in source and binary forms, with or without
 modification, are permitted provided that the following conditions are met:

 - Redistributions of source code must retain the above copyright notice, this
 list of conditions and the following disclaimer.

 - Redistributions in binary form must reproduce the above copyright notice,
 this list of conditions and the following disclaimer in the documentation
 and/or other materials provided with the distribution.

 THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
 FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/
#include "JSC.h"

#define V8_ISOLATE_OBJ(ctx,object,isolate,context,o) \
    V8_ISOLATE_CTX(ctx,isolate,context); \
    Local<Object> o = \
        (object)->L()->ToObject(context).ToLocalChecked();

#define VALUE_ISOLATE(ctxRef,valueRef,isolate,context,value) \
    V8_ISOLATE_CTX(ctxRef,isolate,context); \
    Local<Value> value = (valueRef)->L();

#define V8_ISOLATE_CALLBACK(info,isolate,context,definition) \
    Isolate::Scope isolate_scope_(info.GetIsolate()); \
    HandleScope handle_scope_(info.GetIsolate()); \
    const JSClassDefinition *definition = ObjectData::Get(info.Data())->Definition();\
    if (nullptr == ObjectData::Get(info.Data())->Context()) return; \
    JSContextRef ctxRef_ = ObjectData::Get(info.Data())->Context(); \
    V8_ISOLATE_CTX(ctxRef_->Context(),isolate,context)

#define TO_REAL_GLOBAL(o) \
    o = o->StrictEquals(context->Global()) && \
        !o->GetPrototype()->ToObject(context).IsEmpty() && \
        o->GetPrototype()->ToObject(context).ToLocalChecked()->InternalFieldCount()>INSTANCE_OBJECT_JSOBJECT ? \
        o->GetPrototype()->ToObject(context).ToLocalChecked() : \
        o;

#define CTX(ctx)     ((ctx)->Context())

class ObjectData {
    public:
        static Local<Value> New(const JSClassDefinition *def = nullptr, JSContextRef ctx = nullptr,
                               JSClassRef cls = nullptr) {

            Isolate *isolate = Isolate::GetCurrent();

            ObjectData *od = new ObjectData(def, ctx, cls);
            char ptr[32];
            sprintf(ptr, "%p", od);
            Local<String> data =
                String::NewFromUtf8(isolate, ptr, NewStringType::kNormal).ToLocalChecked();

            UniquePersistent<Value> m_weak = UniquePersistent<Value>(isolate, data);
            m_weak.SetWeak<ObjectData>(
                od,
                [](const WeakCallbackInfo<ObjectData>& info) {

                delete info.GetParameter();
            }, v8::WeakCallbackType::kParameter);

            return data;
        }
        static ObjectData* Get(Local<Value> value) {
            String::Utf8Value const str(value);
            unsigned long n = strtoul(*str, NULL, 16);
            return (ObjectData*) n;
        }
        void SetContext(JSContextRef ctx) {
            m_context = ctx;
        }
        void SetName(Local<Value> name) {
            String::Utf8Value const str(name);
            m_name = (char *) malloc(strlen(*str) + 1);
            strcpy(m_name, *str);
        }
        void SetFunc(Local<Object> func) {
            Isolate *isolate = Isolate::GetCurrent();

            m_func = Persistent<Object,CopyablePersistentTraits<Object>>(isolate, func);
        }
        const JSClassDefinition *Definition() { return m_definition; }
        JSContextRef       Context() { return m_context; }
        JSClassRef         Class() { return m_class; }
        const char *       Name() { return m_name; }
        Local<Object>      Func() { return Local<Object>::New(Isolate::GetCurrent(), m_func); }

        virtual ~ObjectData() {
            if (m_name) free(m_name);
            m_name = nullptr;

            if (m_class) {
                m_class->release();
            }

            m_func.Reset();
        }

    private:
        ObjectData(const JSClassDefinition *def, JSContextRef ctx, JSClassRef cls) :
            m_definition(def), m_context(ctx), m_class(cls), m_name(nullptr) {

            if (cls) cls->retain();
        }

        const JSClassDefinition*m_definition;
        JSContextRef            m_context;
        JSClassRef              m_class;
        UniquePersistent<Value> m_weak;
        char *                  m_name;
        Persistent<Object, CopyablePersistentTraits<Object>> m_func;
};

OpaqueJSClass::OpaqueJSClass(const JSClassDefinition *definition)
{
    m_definition = new JSClassDefinition;
    memcpy(m_definition, definition, sizeof(JSClassDefinition));

    if(m_definition->parentClass) {
        m_definition->parentClass->retain();
    }
}

OpaqueJSClass::~OpaqueJSClass()
{
    if (m_definition->parentClass) {
        m_definition->parentClass->release();
    }

    delete m_definition;
}

void OpaqueJSClass::StaticFunctionCallHandler(const FunctionCallbackInfo< Value > &info)
{
    V8_ISOLATE_CALLBACK(info,isolate,context,definition)
        const char *str = ObjectData::Get(info.Data())->Name();

        JSValueRef arguments[info.Length()];
        for (int i=0; i<info.Length(); i++) {
            arguments[i] = OpaqueJSValue::New(ctxRef_, info[i]);
        }
        TempJSValue function(ctxRef_, ObjectData::Get(info.Data())->Func());

        TempJSValue thisObject(ctxRef_, info.This());

        TempException exception(nullptr);
        TempJSValue value;

        while (definition && !*exception && !*value) {
            for (int i=0; !*value && !*exception &&
                definition->staticFunctions && definition->staticFunctions[i].name;
                i++) {

                if (!strcmp(definition->staticFunctions[i].name, str) &&
                    definition->staticFunctions[i].callAsFunction) {

                    value.Set(definition->staticFunctions[i].callAsFunction(
                        ctxRef_,
                        const_cast<JSObjectRef>(*function),
                        const_cast<JSObjectRef>(*thisObject),
                        (size_t) info.Length(),
                        arguments,
                        &exception));
                }
            }

            definition = definition->parentClass ? definition->parentClass->m_definition : nullptr;
        }

        for (int i=0; i<info.Length(); i++) {
            arguments[i]->Clean();
        }

        if (*exception) {
            isolate->ThrowException((*exception)->L());
        }

        if (*value) {
            info.GetReturnValue().Set((*value)->L());
        }

    V8_UNLOCK()
}

void OpaqueJSClass::ConvertFunctionCallHandler(const FunctionCallbackInfo< Value > &info)
{
    V8_ISOLATE_CALLBACK(info,isolate,context,definition)
        TempJSValue thisObject(ctxRef_, info.This());

        String::Utf8Value const str(info[0]);

        TempException exception(nullptr);
        TempJSValue value;

        JSType type =
            !strcmp("number",  *str) ? kJSTypeNumber :
            !strcmp("string", *str) ? kJSTypeString :
            kJSTypeNumber; // FIXME

        while (definition && !*exception && !*value) {

            if (definition->convertToType) {
                value.Set(definition->convertToType(
                    ctxRef_,
                    const_cast<JSObjectRef>(*thisObject),
                    type,
                    &exception));
            }

            definition = definition->parentClass ? definition->parentClass->m_definition : nullptr;
        }


        if (*exception) {
            isolate->ThrowException((*exception)->L());
        }

        if (*value) {
            info.GetReturnValue().Set((*value)->L());
        }

    V8_UNLOCK()
}

void OpaqueJSClass::HasInstanceFunctionCallHandler(const FunctionCallbackInfo< Value > &info)
{
    V8_ISOLATE_CALLBACK(info,isolate,context,definition)
        TempJSValue thisObject(ctxRef_, info.This());
        TempException exception(nullptr);
        TempJSValue value;
        TempJSValue possibleInstance(ctxRef_, info[0]);

        if (ObjectData::Get(info.Data())->Class() && info[0]->IsObject()) {

            JSClassRef ctor = ObjectData::Get(info.Data())->Class();
            if (info[0].As<Object>()->InternalFieldCount() > INSTANCE_OBJECT_CLASS) {
                JSClassRef inst =
                    (JSClassRef) info[0].As<Object>()->GetAlignedPointerFromInternalField(INSTANCE_OBJECT_CLASS);
                bool has = false;
                for( ; inst && !has; inst = inst->Definition()->parentClass) {
                    has = ctor == inst;
                }
                value.Set(ctxRef_, Boolean::New(isolate, has));
            }
        }

        while (definition && !*exception && !*value) {

            if (definition->hasInstance) {
                bool has = definition->hasInstance(
                    ctxRef_,
                    const_cast<JSObjectRef>(*thisObject),
                    *possibleInstance,
                    &exception);
                value.Set(ctxRef_, Boolean::New(isolate, has));
            }

            definition = definition->parentClass ? definition->parentClass->m_definition : nullptr;
        }

        if (*exception) {
            isolate->ThrowException((*exception)->L());
        }

        if (*value) {
            info.GetReturnValue().Set((*value)->L());
        }
    V8_UNLOCK()
}

void OpaqueJSClass::NamedPropertyQuerier(Local< String > property,
    const PropertyCallbackInfo< Integer > &info)
{
    V8_ISOLATE_CALLBACK(info,isolate,context,definition)
        TempJSValue thisObject(ctxRef_, info.This());

        String::Utf8Value const str(property);
        OpaqueJSString string(*str);

        bool has = false;

        while (definition && !has) {
            if (definition->hasProperty) {
                has = definition->hasProperty(
                    ctxRef_,
                    const_cast<JSObjectRef>(*thisObject),
                    &string);
            }

            if (!has) {
                // check static values
                for (int i=0; !has && definition->staticValues && definition->staticValues[i].name;
                    i++) {

                    if (!strcmp(definition->staticValues[i].name, *str) &&
                        !(definition->staticValues[i].attributes & kJSPropertyAttributeDontEnum)) {
                        has = true;
                    }
                }
            }
            definition = definition->parentClass ? definition->parentClass->m_definition : nullptr;
        }

        if (has) {
            info.GetReturnValue().Set(v8::DontEnum);
        }
    V8_UNLOCK()
}

void OpaqueJSClass::IndexedPropertyQuerier(uint32_t index,
    const PropertyCallbackInfo< Integer > &info)
{
    char prop[50];
    sprintf(prop, "%u", index);
    Isolate::Scope isolate_scope_(info.GetIsolate());
    HandleScope handle_scope_(info.GetIsolate());
    NamedPropertyQuerier(String::NewFromUtf8(info.GetIsolate(),prop), info);
}

void OpaqueJSClass::ProtoPropertyQuerier(Local< String > property,
    const PropertyCallbackInfo< Integer > &info)
{
    V8_ISOLATE_CALLBACK(info,isolate,context,definition)
        String::Utf8Value const str(property);

        bool has = false;

        while (definition && !has) {
                // check static functions
                for (int i=0; !has && definition->staticFunctions &&
                    definition->staticFunctions[i].name;
                    i++) {

                    if (!strcmp(definition->staticFunctions[i].name, *str) &&
                        !(definition->staticFunctions[i].attributes &
                        kJSPropertyAttributeDontEnum)) {
                        has = true;
                    }
                }
            definition = definition->parentClass ? definition->parentClass->m_definition : nullptr;
        }

        if (has) {
            info.GetReturnValue().Set(v8::DontEnum);
        }
    V8_UNLOCK()
}

void OpaqueJSClass::NamedPropertyGetter(Local< String > property,
    const PropertyCallbackInfo< Value > &info)
{
    V8_ISOLATE_CALLBACK(info,isolate,context,definition)
        TempException exception(nullptr);
        TempJSValue thisObject(ctxRef_, info.This());
        TempJSValue value;

        String::Utf8Value const str(property);
        OpaqueJSString string(*str);

        const JSClassDefinition *top = definition;
        while (definition && !*value && !*exception) {
            // Check accessor
            bool hasProperty = true;
            if (definition->hasProperty) {
                hasProperty = definition->hasProperty(
                    ctxRef_,
                    const_cast<JSObjectRef>(*thisObject),
                    &string);
            }
            if (hasProperty && definition->getProperty) {
                value.Set(definition->getProperty(
                    ctxRef_,
                    const_cast<JSObjectRef>(*thisObject),
                    &string,
                    &exception));
            }

            // If this function returns NULL, the get request forwards to object's statically
            // declared properties ...
            // Check static values
            for (int i=0; !*value && !*exception &&
                definition->staticValues && definition->staticValues[i].name;
                i++) {

                if (!strcmp(definition->staticValues[i].name, *str) &&
                    definition->staticValues[i].getProperty) {

                    value.Set(definition->staticValues[i].getProperty(
                        ctxRef_,
                        const_cast<JSObjectRef>(*thisObject),
                        &string,
                        &exception));
                }
            }

            // then its parent class chain (which includes the default object class)
            definition = definition->parentClass ? definition->parentClass->m_definition : nullptr;
        }

        if (!*value && !*exception) {
            definition = top;
            while (definition && !*value && !*exception) {
                if (definition->hasProperty) {
                    bool has = definition->hasProperty(
                        ctxRef_,
                        const_cast<JSObjectRef>(*thisObject),
                        &string);
                    if (has) {
                        // Oops.  We claim to have this property, but we really don't
                        Local<String> error = String::NewFromUtf8(isolate, "Invalid property: ");
                        error = String::Concat(error, property);
                        exception.Set(ctxRef_, Exception::Error(error));
                    }
                }
                definition = definition->parentClass ?definition->parentClass->m_definition:nullptr;
            }
        }

        // ... then its prototype chain.

        if (*exception) {
            isolate->ThrowException((*exception)->L());
        }

        if (*value) {
            info.GetReturnValue().Set((*value)->L());
        }
    V8_UNLOCK()
}

void OpaqueJSClass::IndexedPropertyGetter(uint32_t index,
    const PropertyCallbackInfo< Value > &info)
{
    char prop[50];
    sprintf(prop, "%u", index);
    Isolate::Scope isolate_scope_(info.GetIsolate());
    HandleScope handle_scope_(info.GetIsolate());
    NamedPropertyGetter(String::NewFromUtf8(info.GetIsolate(),prop), info);
}

void OpaqueJSClass::ProtoPropertyGetter(Local< String > property,
    const PropertyCallbackInfo< Value > &info)
{
    V8_ISOLATE_CALLBACK(info,isolate,context,definition)
        TempException exception(nullptr);
        TempJSValue thisObject(ctxRef_, info.This());
        TempJSValue value;

        String::Utf8Value const str(property);
        OpaqueJSString string(*str);

        while (definition && !*value && !*exception) {
            // Check static functions
            for (int i=0; !*value && !*exception &&
                definition->staticFunctions && definition->staticFunctions[i].name;
                i++) {

                if (!strcmp(definition->staticFunctions[i].name, *str) &&
                    definition->staticFunctions[i].callAsFunction) {

                    Local<Value> data = ObjectData::New(definition, ctxRef_);
                    ObjectData::Get(data)->SetName(property);

                    Local<FunctionTemplate> ftempl =
                        FunctionTemplate::New(isolate, StaticFunctionCallHandler, data);
                    Local<Function> func = ftempl->GetFunction();
                    ObjectData::Get(data)->SetFunc(func);
                    value.Set(ctxRef_, func);
                }
            }

            // then its parent class chain (which includes the default object class)
            definition = definition->parentClass ? definition->parentClass->m_definition : nullptr;
        }

        if (*exception) {
            isolate->ThrowException((*exception)->L());
        }

        if (*value) {
            info.GetReturnValue().Set((*value)->L());
        }
    V8_UNLOCK()
}

void OpaqueJSClass::NamedPropertySetter(Local< String > property, Local< Value > value,
    const PropertyCallbackInfo< Value > &info)
{
    String::Utf8Value const str(property);

    V8_ISOLATE_CALLBACK(info,isolate,context,definition)
        TempException exception(nullptr);
        TempJSValue thisObject(ctxRef_, info.This());
        TempJSValue valueRef(ctxRef_,value);

        String::Utf8Value const str(property);
        OpaqueJSString string(*str);

        bool set = false;
        while (definition && !*exception && !set) {
            // Check static values
            for (int i=0; !set && !*exception &&
                definition->staticValues && definition->staticValues[i].name;
                i++) {

                if (!strcmp(definition->staticValues[i].name, *str) &&
                    definition->staticValues[i].setProperty) {

                    set = definition->staticValues[i].setProperty(
                        ctxRef_,
                        const_cast<JSObjectRef>(*thisObject),
                        &string,
                        *valueRef,
                        &exception);
                }
            }

            if( !set && !*exception) {
                if (definition->hasProperty) {
                    // Don't set real property if we are overriding accessors
                    set = definition->hasProperty(
                        ctxRef_,
                        const_cast<JSObjectRef>(*thisObject),
                        &string);
                }

                if (definition->setProperty) {
                    bool reset = definition->setProperty(
                        ctxRef_,
                        const_cast<JSObjectRef>(*thisObject),
                        &string,
                        *valueRef,
                        &exception);
                    set = set || reset;
                }
            }

            definition = definition->parentClass ? definition->parentClass->m_definition : nullptr;
        }

        if (*exception) {
            isolate->ThrowException((*exception)->L());
            value = (*exception)->L();
            set = true;
        }

        if (set) {
            info.GetReturnValue().Set(value);
        }
    V8_UNLOCK()
}

void OpaqueJSClass::IndexedPropertySetter(uint32_t index, Local< Value > value,
    const PropertyCallbackInfo< Value > &info)
{
    char prop[50];
    sprintf(prop, "%u", index);
    Isolate::Scope isolate_scope_(info.GetIsolate());
    HandleScope handle_scope_(info.GetIsolate());
    NamedPropertySetter(String::NewFromUtf8(info.GetIsolate(),prop), value, info);
}

void OpaqueJSClass::NamedPropertyDeleter(Local< String > property,
    const PropertyCallbackInfo< Boolean > &info)
{
    V8_ISOLATE_CALLBACK(info,isolate,context,definition)
        TempException exception(nullptr);

        TempJSValue thisObject(ctxRef_, info.This());

        String::Utf8Value const str(property);
        OpaqueJSString string(*str);

        bool deleted = false;
        while (definition && !*exception && !deleted) {
            if (definition->deleteProperty) {
                deleted = definition->deleteProperty(
                    ctxRef_,
                    const_cast<JSObjectRef>(*thisObject),
                    &string,
                    &exception);
            }
            definition = definition->parentClass ? definition->parentClass->m_definition : nullptr;
        }

        if (*exception) {
            isolate->ThrowException((*exception)->L());
        }
        if (deleted) {
            info.GetReturnValue().Set(deleted);
        }
    V8_UNLOCK()
}

void OpaqueJSClass::IndexedPropertyDeleter(uint32_t index,
    const PropertyCallbackInfo< Boolean > &info)
{
    char prop[50];
    sprintf(prop, "%u", index);
    Isolate::Scope isolate_scope_(info.GetIsolate());
    HandleScope handle_scope_(info.GetIsolate());
    NamedPropertyDeleter(String::NewFromUtf8(info.GetIsolate(),prop), info);
}

void OpaqueJSClass::NamedPropertyEnumerator(const PropertyCallbackInfo< Array > &info)
{
    V8_ISOLATE_CALLBACK(info,isolate,context,definition)
        TempJSValue thisObject(ctxRef_, info.This());

        OpaqueJSPropertyNameAccumulator accumulator;
        while (definition) {
            if (definition->getPropertyNames) {
                definition->getPropertyNames(
                    ctxRef_,
                    const_cast<JSObjectRef>(*thisObject),
                    &accumulator);
            }

            // Add static values
            for (int i=0; definition->staticValues &&
                definition->staticValues[i].name; i++) {

                if (!(definition->staticValues[i].attributes &
                    kJSPropertyAttributeDontEnum)) {

                    JSStringRef property =
                        JSStringCreateWithUTF8CString(definition->staticValues[i].name);
                    JSPropertyNameAccumulatorAddName(&accumulator, property);
                    JSStringRelease(property);
                }
            }

            definition = definition->parentClass ? definition->parentClass->m_definition : nullptr;
        }

        Local<Array> array = Array::New(isolate);
        Local<Function> indexOf = array->Get(context, String::NewFromUtf8(isolate, "indexOf"))
                .ToLocalChecked().As<Function>();
        Local<Function> push = array->Get(context, String::NewFromUtf8(isolate, "push"))
                .ToLocalChecked().As<Function>();
        while (accumulator.size() > 0) {
            Local<Value> property = accumulator.back()->Value(isolate);
            Local<Value> index = indexOf->Call(context, array, 1, &property).ToLocalChecked();
            if (index->ToNumber(context).ToLocalChecked()->Value() < 0) {
                push->Call(context, array, 1, &property);
            }
            accumulator.back()->release();
            accumulator.pop_back();
        }

        info.GetReturnValue().Set(array);
    V8_UNLOCK()
}

void OpaqueJSClass::IndexedPropertyEnumerator(const PropertyCallbackInfo< Array > &info)
{
    V8_ISOLATE_CALLBACK(info,isolate,context,definition)
        TempJSValue thisObject(ctxRef_, info.This());

        OpaqueJSPropertyNameAccumulator accumulator;
        while (definition) {
            if (definition->getPropertyNames) {
                definition->getPropertyNames(
                    ctxRef_,
                    const_cast<JSObjectRef>(*thisObject),
                    &accumulator);
            }

            // Add static values
            for (int i=0; definition->staticValues &&
                definition->staticValues[i].name; i++) {

                if (!(definition->staticValues[i].attributes &
                    kJSPropertyAttributeDontEnum)) {

                    JSStringRef property =
                        JSStringCreateWithUTF8CString(definition->staticValues[i].name);
                    JSPropertyNameAccumulatorAddName(&accumulator, property);
                    JSStringRelease(property);
                }
            }

            definition = definition->parentClass ? definition->parentClass->m_definition : nullptr;
        }

        Local<Array> array = Array::New(isolate);
        Local<Function> indexOf = array->Get(context, String::NewFromUtf8(isolate, "indexOf"))
                .ToLocalChecked().As<Function>();
        Local<Function> sort = array->Get(context, String::NewFromUtf8(isolate, "sort"))
                .ToLocalChecked().As<Function>();
        Local<Function> push = array->Get(context, String::NewFromUtf8(isolate, "push"))
                .ToLocalChecked().As<Function>();
        Local<Function> isNaN= context->Global()->Get(context, String::NewFromUtf8(isolate,"isNaN"))
                .ToLocalChecked().As<Function>();
        Local<Object> Number= context->Global()->Get(context, String::NewFromUtf8(isolate,"Number"))
                .ToLocalChecked()->ToObject(context).ToLocalChecked();
        Local<Function> isInteger = Number->Get(context, String::NewFromUtf8(isolate,"isInteger"))
                .ToLocalChecked().As<Function>();
        while (accumulator.size() > 0) {
            Local<Value> property = accumulator.back()->Value(isolate);
            Local<Value> numeric = property->ToNumber(context).ToLocalChecked();
            if (!isNaN->Call(context, isNaN, 1, &property).ToLocalChecked()
                ->ToBoolean(context).ToLocalChecked()->Value() &&
                isInteger->Call(context, isInteger, 1, &numeric).ToLocalChecked()
                ->ToBoolean(context).ToLocalChecked()->Value()) {

                Local<Value> index = indexOf->Call(context, array, 1,&numeric).ToLocalChecked();
                if (index->ToNumber(context).ToLocalChecked()->Value() < 0) {
                    push->Call(context, array, 1, &numeric);
                }
            }
            accumulator.back()->release();
            accumulator.pop_back();
        }
        sort->Call(context, array, 0, nullptr);

        info.GetReturnValue().Set(array);
    V8_UNLOCK()
}

void OpaqueJSClass::ProtoPropertyEnumerator(const PropertyCallbackInfo< Array > &info)
{
    V8_ISOLATE_CALLBACK(info,isolate,context,definition)
        TempJSValue thisObject(ctxRef_, info.This());

        OpaqueJSPropertyNameAccumulator accumulator;
        while (definition) {
            // Add static functions
            for (int i=0; definition->staticFunctions &&
                definition->staticFunctions[i].name; i++) {

                if (!(definition->staticFunctions[i].attributes &
                    kJSPropertyAttributeDontEnum)) {

                    JSStringRef property =
                        JSStringCreateWithUTF8CString(definition->staticFunctions[i].name);
                    JSPropertyNameAccumulatorAddName(&accumulator, property);
                    JSStringRelease(property);
                }
            }

            definition = definition->parentClass ? definition->parentClass->m_definition : nullptr;
        }

        Local<Array> array = Array::New(isolate);
        Local<Function> indexOf = array->Get(context, String::NewFromUtf8(isolate, "indexOf"))
                .ToLocalChecked().As<Function>();
        Local<Function> push = array->Get(context, String::NewFromUtf8(isolate, "push"))
                .ToLocalChecked().As<Function>();
        while (accumulator.size() > 0) {
            Local<Value> property = accumulator.back()->Value(isolate);
            Local<Value> index = indexOf->Call(context, array, 1, &property).ToLocalChecked();
            if (index->ToNumber(context).ToLocalChecked()->Value() < 0) {
                push->Call(context, array, 1, &property);
            }
            accumulator.back()->release();
            accumulator.pop_back();
        }

        info.GetReturnValue().Set(array);
    V8_UNLOCK()
}

void OpaqueJSClass::CallAsFunction(const FunctionCallbackInfo< Value > &info)
{
    V8_ISOLATE_CALLBACK(info,isolate,context,definition)
        TempException exception(nullptr);

        JSValueRef arguments[info.Length()];
        for (int i=0; i<info.Length(); i++) {
            arguments[i] = OpaqueJSValue::New(ctxRef_, info[i]);
        }
        TempJSValue function(ctxRef_, ObjectData::Get(info.Data())->Func());
        TempJSValue thisObject(ctxRef_, info.This());

        TempJSValue value;
        while (definition && !*exception && !*value) {
            if (info.IsConstructCall() && definition->callAsConstructor) {
                value.Set(definition->callAsConstructor(
                    ctxRef_,
                    const_cast<JSObjectRef>(*function),
                    (size_t) info.Length(),
                    arguments,
                    &exception));
                if (!*value || !(*value)->L()->IsObject()) {
                    value.Reset();
                    Local<String> error = String::NewFromUtf8(isolate, "Bad constructor");
                    exception.Set(ctxRef_, Exception::Error(error));
                }
            } else if (info.IsConstructCall() && ObjectData::Get(info.Data())->Class()) {
                value.Set(
                    JSObjectMake(ctxRef_, ObjectData::Get(info.Data())->Class(), nullptr));
            } else if (!info.IsConstructCall() && definition->callAsFunction) {
                value.Set(definition->callAsFunction(
                    ctxRef_,
                    const_cast<JSObjectRef>(*function),
                    const_cast<JSObjectRef>(*thisObject),
                    (size_t) info.Length(),
                    arguments,
                    &exception));
            }
            definition = definition->parentClass ? definition->parentClass->m_definition : nullptr;
        }

        for (int i=0; i<info.Length(); i++) {
            arguments[i]->Clean();
        }

        if (*exception) {
            isolate->ThrowException((*exception)->L());
        }

        if (*value) {
            info.GetReturnValue().Set((*value)->L());
        }
    V8_UNLOCK()
}

void OpaqueJSClass::Finalize(const WeakCallbackInfo<UniquePersistent<Object>>& info)
{
    OpaqueJSClass* clazz = reinterpret_cast<OpaqueJSClass*>(info.GetInternalField(INSTANCE_OBJECT_CLASS));
    JSObjectRef objRef =
        reinterpret_cast<JSObjectRef>(info.GetInternalField(INSTANCE_OBJECT_JSOBJECT));
    /* Note: A weak callback will only retain the first two internal fields
     * But the first one is reserved.  So we will have nulled out the second one in the
     * OpaqueJSValue destructor.
     * I am intentionally not using the macro here to ensure that we always
     * read position one, even if the indices move later.
     */
    if ((info.GetInternalField(1) != nullptr) && objRef && !objRef->HasFinalized()) {
        objRef->SetFinalized();
        const JSClassDefinition *definition = clazz ? clazz->m_definition : nullptr;
        while (definition) {
            if (definition->finalize) {
                definition->finalize(objRef);
            }
            definition = definition->parentClass ? definition->parentClass->m_definition : nullptr;
        }
    }
    info.GetParameter()->Reset();
    delete info.GetParameter();
}

bool OpaqueJSClass::IsFunction()
{
    const JSClassDefinition *definition = m_definition;
    while (definition) {
        if (definition->callAsFunction) {
            return true;
        }
        definition = definition->parentClass ? definition->parentClass->m_definition : nullptr;
    }
    return false;
}

bool OpaqueJSClass::IsConstructor()
{
    const JSClassDefinition *definition = m_definition;
    while (definition) {
        if (definition->callAsConstructor) {
            return true;
        }
        definition = definition->parentClass ? definition->parentClass->m_definition : nullptr;
    }
    return false;
}

JSGlobalContextRef OpaqueJSClass::NewContext(JSContextGroupRef group) {
    JSGlobalContextRef ctx = nullptr;

    V8_ISOLATE((ContextGroup*)group, isolate)
        Local<Value> data = ObjectData::New(m_definition);

        Local<ObjectTemplate> object = ObjectTemplate::New(isolate);

        object->SetNamedPropertyHandler(
            NamedPropertyGetter,
            NamedPropertySetter,
            NamedPropertyQuerier,
            NamedPropertyDeleter,
            NamedPropertyEnumerator,
            data);

        object->SetIndexedPropertyHandler(
            IndexedPropertyGetter,
            IndexedPropertySetter,
            IndexedPropertyQuerier,
            IndexedPropertyDeleter,
            IndexedPropertyEnumerator,
            data);

        if (IsFunction() || IsConstructor()) {
            object->SetCallAsFunctionHandler(CallAsFunction, data);
        }

        object->SetInternalFieldCount(INSTANCE_OBJECT_FIELDS);

        Local<Context> context = Context::New(isolate, nullptr, object);
        {
            Context::Scope context_scope_(context);
            Local<Object> global =
                context->Global()->GetPrototype()->ToObject(context).ToLocalChecked();
            ctx = new OpaqueJSContext(new JSContext((ContextGroup*)group, context));
            TempJSValue value(InitInstance(ctx, global, data, nullptr));
        }
    V8_UNLOCK()

    return ctx;
}

void OpaqueJSClass::NewTemplate(Local<Context> context, Local<Value> *data,
    Local<ObjectTemplate> *object)
{
    Isolate *isolate = context->GetIsolate();

    Context::Scope context_scope_(context);

    *object = ObjectTemplate::New(isolate);

    // Create data object
    *data = ObjectData::New(m_definition);

    (*object)->SetNamedPropertyHandler(
        NamedPropertyGetter,
        NamedPropertySetter,
        NamedPropertyQuerier,
        NamedPropertyDeleter,
        NamedPropertyEnumerator,
        *data);

    (*object)->SetIndexedPropertyHandler(
        IndexedPropertyGetter,
        IndexedPropertySetter,
        IndexedPropertyQuerier,
        IndexedPropertyDeleter,
        IndexedPropertyEnumerator,
        *data);

    if (IsFunction() || IsConstructor()) {
        (*object)->SetCallAsFunctionHandler(CallAsFunction, *data);
    }

    (*object)->SetInternalFieldCount(INSTANCE_OBJECT_FIELDS);
}

JSObjectRef OpaqueJSClass::InitInstance(JSContextRef ctx, Local<Object> instance,
    Local<Value> data, void *privateData)
{
    JSObjectRef retObj;

    V8_ISOLATE_CTX(CTX(ctx),isolate,context)
        ObjectData::Get(data)->SetContext(ctx);

        if (IsFunction() || IsConstructor()) {
            ObjectData::Get(data)->SetFunc(instance);
        }
        if (IsFunction()) {
            retObj = OpaqueJSValue::New(ctx, instance.As<Function>(), m_definition);
        } else {
            retObj = OpaqueJSValue::New(ctx, instance, m_definition);
        }

        UniquePersistent<Object>* weak = new UniquePersistent<Object>(isolate, instance);
        weak->SetWeak<UniquePersistent<Object>>(
            weak,
            Finalize,
            v8::WeakCallbackType::kInternalFields);

        instance->SetAlignedPointerInInternalField(INSTANCE_OBJECT_CLASS,this);
        retain();
        instance->SetAlignedPointerInInternalField(INSTANCE_OBJECT_JSOBJECT,(void*)retObj);
        retObj->SetPrivateData(privateData);

        // Set up a prototype object to handle static functions
        Local<ObjectTemplate> protoTemplate = ObjectTemplate::New(isolate);
        protoTemplate->SetNamedPropertyHandler(
            ProtoPropertyGetter,
            nullptr,
            ProtoPropertyQuerier,
            nullptr,
            ProtoPropertyEnumerator,
            data);
        Local<Object> prototype = protoTemplate->NewInstance(context).ToLocalChecked();
        instance->SetPrototype(context, prototype);

        // Set className
        const JSClassDefinition *definition = m_definition;
        while (true) {
            const char* sClassName = definition ? definition->className : "CallbackObject";
            if (sClassName) {
                Local<String> className = String::NewFromUtf8(isolate, sClassName);
                Local<Object> Symbol =
                    context->Global()->Get(String::NewFromUtf8(isolate, "Symbol"))->ToObject();
                Local<Value> toStringTag = Symbol->Get(String::NewFromUtf8(isolate, "toStringTag"));
                prototype->Set(context, toStringTag, className);
                break;
            }
            if (!definition) break;
            definition = definition->parentClass ? definition->parentClass->m_definition : nullptr;
        }

        // Override @@toPrimitive if convertToType set
        definition = m_definition;
        while (definition) {
            if (definition->convertToType) {
                Local<FunctionTemplate> ftempl = FunctionTemplate::New(isolate,
                    ConvertFunctionCallHandler, data);
                Local<Function> function = ftempl->GetFunction(context).ToLocalChecked();
                Local<Object> Symbol =
                    context->Global()->Get(String::NewFromUtf8(isolate, "Symbol"))->ToObject();
                Local<Value> toPrimitive = Symbol->Get(String::NewFromUtf8(isolate, "toPrimitive"));
                prototype->Set(context, toPrimitive, function);
                break;
            }
            definition = definition->parentClass ? definition->parentClass->m_definition : nullptr;
        }

        // Override @@hasInstance if hasInstance set
        definition = m_definition;
        while (definition) {
            if (definition->hasInstance) {
                Local<FunctionTemplate> ftempl = FunctionTemplate::New(isolate,
                    HasInstanceFunctionCallHandler, data);
                Local<Function> function = ftempl->GetFunction(context).ToLocalChecked();
                Local<Object> Symbol =
                    context->Global()->Get(String::NewFromUtf8(isolate, "Symbol"))->ToObject();
                Local<Value> hasInstance = Symbol->Get(String::NewFromUtf8(isolate, "hasInstance"));
                prototype->Set(context, hasInstance, function);
                break;
            }
            definition = definition->parentClass ? definition->parentClass->m_definition : nullptr;
        }

        // Find the greatest ancestor
        definition = nullptr;
        for (definition = m_definition; definition && definition->parentClass;
            definition = definition->parentClass->m_definition);

        // Walk backwards and call 'initialize' on each
        while (true) {
            if (definition->initialize)
                definition->initialize(ctx, retObj);
            const JSClassDefinition *parent = definition;
            if (parent == m_definition) break;

            for (definition = m_definition;
                definition->parentClass && definition->parentClass->m_definition != parent;
                definition = definition->parentClass->m_definition);
        }
    V8_UNLOCK()

    return retObj;
}

const JSClassDefinition kJSClassDefinitionEmpty = {
    0, 0,
    nullptr,
    nullptr, nullptr,
    nullptr, nullptr, nullptr,
    nullptr, nullptr, nullptr, nullptr,
    nullptr, nullptr, nullptr, nullptr, nullptr
};

JS_EXPORT JSClassRef JSClassCreate(const JSClassDefinition* definition)
{
    return new OpaqueJSClass(definition);
}

JS_EXPORT JSClassRef JSClassRetain(JSClassRef jsClass)
{
    jsClass->retain();
    return jsClass;
}

JS_EXPORT void JSClassRelease(JSClassRef jsClass)
{
    jsClass->release();
}

JS_EXPORT JSObjectRef JSObjectMake(JSContextRef ctx, JSClassRef jsClass, void* data)
{
    JSObjectRef value;

    V8_ISOLATE_CTX(CTX(ctx),isolate,context)
        if (jsClass) {
            Local<Value> payload;
            Local<ObjectTemplate> templ;
            jsClass->NewTemplate(context, &payload, &templ);
            Local<Object> instance = templ->NewInstance(context).ToLocalChecked();
            value = jsClass->InitInstance(ctx, instance, payload, data);
        } else {
            value = OpaqueJSValue::New(ctx, Object::New(isolate));
        }
    V8_UNLOCK()

    return value;
}

static JSObjectRef SetUpFunction(JSContextRef ctx, JSStringRef name, JSClassDefinition *definition,
    JSClassRef jsClass, bool isConstructor)
{
    JSObjectRef obj;
    V8_ISOLATE_CTX(CTX(ctx),isolate,context)
        Local<Value> data = ObjectData::New(definition, ctx, jsClass);

        Local<Object> func;

        if (!isConstructor) {
            Local<FunctionTemplate> ftempl = FunctionTemplate::New(isolate,
                OpaqueJSClass::CallAsFunction, data);
            Local<Function> f = ftempl->GetFunction();
            if (name) {
                f->SetName(name->Value(isolate));
            }
            func = f;
        } else {
            Local<ObjectTemplate> ctempl = ObjectTemplate::New(isolate);
            ctempl->SetCallAsFunctionHandler(OpaqueJSClass::CallAsFunction, data);
            func = ctempl->NewInstance(context).ToLocalChecked();
        }

        if (jsClass) {
            Local<FunctionTemplate> ftempl = FunctionTemplate::New(isolate,
                OpaqueJSClass::HasInstanceFunctionCallHandler, data);
            Local<Function> function = ftempl->GetFunction(context).ToLocalChecked();
            Local<Object> Symbol =
                context->Global()->Get(String::NewFromUtf8(isolate, "Symbol"))->ToObject();
            Local<Value> hasInstance = Symbol->Get(String::NewFromUtf8(isolate, "hasInstance"));
            Local<Object> prototype = Object::New(isolate);
            prototype->Set(context, hasInstance, function);
            func->SetPrototype(context, prototype);
        }

        ObjectData::Get(data)->SetFunc(func);

        obj = OpaqueJSValue::New(ctx, func);
    V8_UNLOCK()

    return obj;
}

JS_EXPORT JSObjectRef JSObjectMakeFunctionWithCallback(JSContextRef ctx, JSStringRef name,
    JSObjectCallAsFunctionCallback callAsFunction)
{
    JSClassDefinition *definition = new JSClassDefinition;
    memset(definition, 0, sizeof(JSClassDefinition));
    definition->callAsFunction = callAsFunction;

    return SetUpFunction(ctx, name, definition, nullptr, false);
}

JS_EXPORT JSObjectRef JSObjectMakeConstructor(JSContextRef ctx, JSClassRef jsClass,
    JSObjectCallAsConstructorCallback callAsConstructor)
{
    JSClassDefinition *definition = new JSClassDefinition;
    memset(definition, 0, sizeof(JSClassDefinition));
    definition->callAsConstructor = callAsConstructor;

    return SetUpFunction(ctx, nullptr, definition, jsClass, true);
}

JS_EXPORT JSObjectRef JSObjectMakeArray(JSContextRef ctx, size_t argumentCount,
    const JSValueRef arguments[], JSValueRef* exception)
{
    JSObjectRef object;

    V8_ISOLATE_CTX(CTX(ctx),isolate,context)
        Local<Array> array = Array::New(isolate, argumentCount);
        for(size_t i=0; i<argumentCount; i++) {
            array->Set(context, i, arguments[i]->L());
        }
        object = OpaqueJSValue::New(ctx, array);
    V8_UNLOCK()

    return object;
}

JS_EXPORT JSObjectRef JSObjectMakeDate(JSContextRef ctx, size_t argumentCount,
    const JSValueRef arguments[], JSValueRef* exceptionRef)
{
    JSObjectRef out;

    V8_ISOLATE_CTX(CTX(ctx),isolate,context)
        TempException exception(exceptionRef);
        Local<Value> date;
        if (argumentCount==0) {
            Local<Object> DATE =
                context->Global()->Get(String::NewFromUtf8(isolate, "Date"))->ToObject();
            Local<Function> now = DATE->Get(String::NewFromUtf8(isolate, "now")).As<Function>();

            date = Date::New(isolate,
                now->Call(Local<Value>::New(isolate,Null(isolate)), 0, nullptr)
                    ->ToNumber(context).ToLocalChecked()->Value());
        } else {
            TryCatch trycatch(isolate);

            MaybeLocal<Number> number = arguments[0]->L()->ToNumber(context);
            double epoch = 0.0;
            if (!number.IsEmpty()) {
                epoch = number.ToLocalChecked()->Value();
            } else {
                exception.Set(ctx, trycatch.Exception());
                epoch = 0.0;
            }

            date = Date::New(isolate, epoch);
        }

        out = OpaqueJSValue::New(ctx, date);
    V8_UNLOCK()

    return out;

}

JS_EXPORT JSObjectRef JSObjectMakeError(JSContextRef ctx, size_t argumentCount,
    const JSValueRef arguments[], JSValueRef* exceptionRef)
{
    JSObjectRef out;
    V8_ISOLATE_CTX(CTX(ctx),isolate,context)
        TempException exception(exceptionRef);

        Local<String> str =
            String::NewFromUtf8(isolate, "", NewStringType::kNormal).ToLocalChecked();

        if (argumentCount>0) {
            TryCatch trycatch(isolate);
            MaybeLocal<String> maybe = arguments[0]->L()->ToString(context);
            if (!maybe.IsEmpty()) {
                str = maybe.ToLocalChecked();
            } else {
                exception.Set(ctx, trycatch.Exception());
            }
        }

        out = OpaqueJSValue::New(ctx, Exception::Error(str));
    V8_UNLOCK()

    return out;

}

JS_EXPORT JSObjectRef JSObjectMakeRegExp(JSContextRef ctx, size_t argumentCount,
    const JSValueRef arguments[], JSValueRef* exceptionRef)
{
    JSObjectRef out = nullptr;

    V8_ISOLATE_CTX(CTX(ctx),isolate,context)
        TempException exception(exceptionRef);
        Local<String> pattern =
            String::NewFromUtf8(isolate, "", NewStringType::kNormal).ToLocalChecked();
        Local<String> flags_ =
            String::NewFromUtf8(isolate, "", NewStringType::kNormal).ToLocalChecked();

        if (argumentCount > 0) {
            TryCatch trycatch(isolate);
            MaybeLocal<String> maybe = arguments[0]->L()->ToString(context);
            if (!maybe.IsEmpty()) {
                pattern = maybe.ToLocalChecked();
            } else {
                exception.Set(ctx, trycatch.Exception());
            }
        }

        if (!*exception && argumentCount > 1) {
            TryCatch trycatch(isolate);
            MaybeLocal<String> maybe = arguments[1]->L()->ToString(context);
            if (!maybe.IsEmpty()) {
                flags_ = maybe.ToLocalChecked();
            } else {
                exception.Set(ctx, trycatch.Exception());
            }
        }

        if (!*exception) {
            String::Utf8Value const str(flags_);
            RegExp::Flags flags = RegExp::Flags::kNone;
            for (size_t i=0; i<strlen(*str); i++) {
                switch ((*str)[i]) {
                    case 'g': flags = (RegExp::Flags) (flags | RegExp::Flags::kGlobal);     break;
                    case 'i': flags = (RegExp::Flags) (flags | RegExp::Flags::kIgnoreCase); break;
                    case 'm': flags = (RegExp::Flags) (flags | RegExp::Flags::kMultiline);  break;
                }
            }

            TryCatch trycatch(isolate);

            MaybeLocal<RegExp> regexp = RegExp::New(context, pattern, flags);
            if (regexp.IsEmpty()) {
                exception.Set(ctx, trycatch.Exception());
            } else {
                out = OpaqueJSValue::New(ctx, regexp.ToLocalChecked());
            }
        }
    V8_UNLOCK()

    return out;

}

JS_EXPORT JSObjectRef JSObjectMakeFunction(JSContextRef ctx, JSStringRef name,
    unsigned parameterCount, const JSStringRef parameterNames[], JSStringRef body,
    JSStringRef sourceURL, int startingLineNumber, JSValueRef* exceptionRef)
{
    JSObjectRef out = nullptr;

    V8_ISOLATE_CTX(CTX(ctx),isolate,context)
        TempException exception(exceptionRef);
        OpaqueJSString anonymous("anonymous");

        TryCatch trycatch(isolate);

        Local<String> source = String::NewFromUtf8(isolate, "(function ");
        if (name) {
            source = String::Concat(source, name->Value(isolate));
        }
        source = String::Concat(source, String::NewFromUtf8(isolate, "("));
        Local<String> comma = String::NewFromUtf8(isolate, ",");
        for (unsigned i=0; i<parameterCount; i++) {
            source = String::Concat(source, parameterNames[i]->Value(isolate));
            if (i+1 < parameterCount) {
                source = String::Concat(source, comma);
            }
        }
        source = String::Concat(source, String::NewFromUtf8(isolate, ") { "));
        if (body) {
            source = String::Concat(source, body->Value(isolate));
        }
        source = String::Concat(source, String::NewFromUtf8(isolate, "\n})"));

        ScriptOrigin script_origin(
            sourceURL ? sourceURL->Value(isolate) : anonymous.Value(isolate),
            Integer::New(isolate, startingLineNumber)
        );

        MaybeLocal<Script> script = Script::Compile(context, source, &script_origin);
        if (script.IsEmpty()) {
            exception.Set(ctx, trycatch.Exception());
        }

        MaybeLocal<Value> result;

        if (!*exception) {
            result = script.ToLocalChecked()->Run(context);
            if (result.IsEmpty()) {
                exception.Set(ctx, trycatch.Exception());
            }
        }

        if (!*exception) {
            Local<Function> function = Local<Function>::Cast(result.ToLocalChecked());
            if (name) {
                function->SetName(name->Value(isolate));
            }
            out = OpaqueJSValue::New(ctx, result.ToLocalChecked());
        }
    V8_UNLOCK()

    return out;
}

JS_EXPORT JSValueRef JSObjectGetPrototype(JSContextRef ctx, JSObjectRef object)
{
    JSValueRef out = nullptr;

    V8_ISOLATE_OBJ(CTX(ctx),object,isolate,context,o)
        TO_REAL_GLOBAL(o);
        out = OpaqueJSValue::New(ctx, o->GetPrototype());
    V8_UNLOCK()

    return out;
}

JS_EXPORT void JSObjectSetPrototype(JSContextRef ctx, JSObjectRef object, JSValueRef value)
{
    V8_ISOLATE_OBJ(CTX(ctx),object,isolate,context,o)
        TempJSValue null(JSValueMakeNull(ctx));
        if (!value) value = *null;
        TO_REAL_GLOBAL(o);
        o->SetPrototype(context, value->L());
    V8_UNLOCK()
}

JS_EXPORT bool JSObjectHasProperty(JSContextRef ctx, JSObjectRef object, JSStringRef propertyName)
{
    if (!propertyName) return false;
    bool v;

    V8_ISOLATE_OBJ(CTX(ctx),object,isolate,context,o)
        Maybe<bool> has = o->Has(context, propertyName->Value(isolate));
        v = has.FromMaybe(false);
    V8_UNLOCK()

    return v;
}

JS_EXPORT JSValueRef JSObjectGetProperty(JSContextRef ctx, JSObjectRef object,
    JSStringRef propertyName, JSValueRef* exceptionRef)
{
    JSValueRef out = nullptr;

    V8_ISOLATE_OBJ(CTX(ctx),object,isolate,context,o)
        TempException exception(exceptionRef);
        TryCatch trycatch(isolate);

        MaybeLocal<Value> value = o->Get(context, propertyName->Value(isolate));
        if (value.IsEmpty()) {
            exception.Set(ctx, trycatch.Exception());
        }

        if (!*exception) {
            out = OpaqueJSValue::New(ctx, value.ToLocalChecked());
        }
    V8_UNLOCK()

    return out;
}

JS_EXPORT void JSObjectSetProperty(JSContextRef ctx, JSObjectRef object, JSStringRef propertyName,
    JSValueRef value, JSPropertyAttributes attributes, JSValueRef* exceptionRef)
{
    V8_ISOLATE_OBJ(CTX(ctx),object,isolate,context,o)
        TempException exception(exceptionRef);
        TempJSValue null(JSValueMakeNull(ctx));
        if (!value) value = *null;
        int v8_attr = v8::None;
        if (attributes & kJSPropertyAttributeReadOnly) v8_attr |= v8::ReadOnly;
        if (attributes & kJSPropertyAttributeDontEnum) v8_attr |= v8::DontEnum;
        if (attributes & kJSPropertyAttributeDontDelete) v8_attr |= v8::DontDelete;

        TryCatch trycatch(isolate);

        Maybe<bool> defined = (attributes!=0) ?
            o->DefineOwnProperty(
                context,
                propertyName->Value(isolate),
                value->L(),
                static_cast<PropertyAttribute>(v8_attr))
            :
            o->Set(context, propertyName->Value(isolate), value->L());

        if (defined.IsNothing()) {
            exception.Set(ctx, trycatch.Exception());
        }
    V8_UNLOCK()
}

JS_EXPORT bool JSObjectDeleteProperty(JSContextRef ctx, JSObjectRef object,
    JSStringRef propertyName, JSValueRef* exceptionRef)
{
    if (!propertyName) return false;

    bool v = false;
    V8_ISOLATE_OBJ(CTX(ctx),object,isolate,context,o)
        TempException exception(exceptionRef);

        TryCatch trycatch(isolate);

        Maybe<bool> deleted = o->Delete(context, propertyName->Value(isolate));
        if (deleted.IsNothing()) {
            exception.Set(ctx, trycatch.Exception());
        } else {
            v = deleted.FromMaybe(false);
        }
    V8_UNLOCK()

    return v;
}

JS_EXPORT JSValueRef JSObjectGetPropertyAtIndex(JSContextRef ctx, JSObjectRef object,
    unsigned propertyIndex, JSValueRef* exceptionRef)
{
    JSValueRef out = nullptr;

    V8_ISOLATE_OBJ(CTX(ctx),object,isolate,context,o)
        TempException exception(exceptionRef);
        TryCatch trycatch(isolate);

        MaybeLocal<Value> value = o->Get(context, propertyIndex);
        if (value.IsEmpty()) {
            exception.Set(ctx, trycatch.Exception());
        }

        if (!*exception) {
            out = OpaqueJSValue::New(ctx, value.ToLocalChecked());
        }
    V8_UNLOCK()

    return out;
}

JS_EXPORT void JSObjectSetPropertyAtIndex(JSContextRef ctx, JSObjectRef object,
    unsigned propertyIndex, JSValueRef value, JSValueRef* exceptionRef)
{
    V8_ISOLATE_OBJ(CTX(ctx),object,isolate,context,o)
        TempException exception(exceptionRef);
        TempJSValue null(JSValueMakeNull(ctx));
        if (!value) value = *null;
        TryCatch trycatch(isolate);

        Maybe<bool> defined =
            o->Set(context, propertyIndex, value->L());

        if (defined.IsNothing()) {
            exception.Set(ctx, trycatch.Exception());
        }
    V8_UNLOCK()
}

JS_EXPORT void* JSObjectGetPrivate(JSObjectRef object)
{
    return object->GetPrivateData();
}

JS_EXPORT bool JSObjectSetPrivate(JSObjectRef object, void* data)
{
    return object->SetPrivateData(data);
}

JS_EXPORT bool JSObjectIsFunction(JSContextRef ctx, JSObjectRef object)
{
    if (!object) return false;
    bool v;

    VALUE_ISOLATE(CTX(ctx),object,isolate,context,value)
        v = value->IsFunction();
    V8_UNLOCK()

    return v;
}

JS_EXPORT JSValueRef JSObjectCallAsFunction(JSContextRef ctx, JSObjectRef object,
    JSObjectRef thisObject, size_t argumentCount, const JSValueRef arguments[],
    JSValueRef* exceptionRef)
{
    if (!object) return nullptr;

    JSValueRef out = nullptr;

    V8_ISOLATE_OBJ(CTX(ctx),object,isolate,context,o)
        TempException exception(exceptionRef);
        Local<Value> this_ = thisObject ?
            thisObject->L() :
            Local<Value>::New(isolate,Null(isolate));

        Local<Value> *elements = new Local<Value>[argumentCount];
        for (size_t i=0; i<argumentCount; i++) {
            if (arguments[i]) {
                elements[i] = arguments[i]->L();
            } else {
                elements[i] = Local<Value>::New(isolate,Null(isolate));
            }
        }

        TryCatch trycatch(isolate);

        MaybeLocal<Value> value = o->CallAsFunction(context, this_, argumentCount, elements);
        if (value.IsEmpty()) {
            exception.Set(ctx, trycatch.Exception());
        }

        if (!*exception) {
            out = OpaqueJSValue::New(ctx, value.ToLocalChecked());
        }
        delete [] elements;
    V8_UNLOCK()

    return out;
}

JS_EXPORT bool JSObjectIsConstructor(JSContextRef ctx, JSObjectRef object)
{
    return JSObjectIsFunction(ctx, object);
}

JS_EXPORT JSObjectRef JSObjectCallAsConstructor(JSContextRef ctx, JSObjectRef object,
    size_t argumentCount, const JSValueRef arguments[], JSValueRef* exceptionRef)
{
    if (!object) return nullptr;

    JSObjectRef out = nullptr;

    V8_ISOLATE_OBJ(CTX(ctx),object,isolate,context,o)
        TempException exception(exceptionRef);
        Local<Value> *elements = new Local<Value>[argumentCount];
        for (size_t i=0; i<argumentCount; i++) {
            elements[i] = arguments[i]->L();
        }

        TryCatch trycatch(isolate);

        MaybeLocal<Value> value = o->CallAsConstructor(context, argumentCount, elements);
        if (value.IsEmpty()) {
            exception.Set(ctx, trycatch.Exception());
        }

        if (!*exception) {
            out = OpaqueJSValue::New(ctx, value.ToLocalChecked());
        }
        delete [] elements;
    V8_UNLOCK()

    return out;
}

JS_EXPORT JSPropertyNameArrayRef JSObjectCopyPropertyNames(JSContextRef ctx, JSObjectRef object)
{
    if (!object) return nullptr;

    JSPropertyNameArrayRef array;

    V8_ISOLATE_OBJ(CTX(ctx),object,isolate,context,o)
        Local<Array> names = o->GetPropertyNames(context).ToLocalChecked();
        array = OpaqueJSValue::New(ctx, names);
        array->Retain();
    V8_UNLOCK()

    return array;
}

JS_EXPORT JSPropertyNameArrayRef JSPropertyNameArrayRetain(JSPropertyNameArrayRef array)
{
    if (!array) return nullptr;

    V8_ISOLATE_CTX(CTX(array->Context()),isolate,context)
        array->Retain();
    V8_UNLOCK()

    return array;
}

JS_EXPORT void JSPropertyNameArrayRelease(JSPropertyNameArrayRef array)
{
    if (!array) return;

    V8_ISOLATE_CTX(CTX(array->Context()),isolate,context)
        array->Release();
    V8_UNLOCK()
}

JS_EXPORT size_t JSPropertyNameArrayGetCount(JSPropertyNameArrayRef array)
{
    if (!array) return 0;

    size_t size;

    V8_ISOLATE_CTX(CTX(array->Context()),isolate,context)
        size = array->L().As<Array>()->Length();
    V8_UNLOCK()

    return size;
}

JS_EXPORT JSStringRef JSPropertyNameArrayGetNameAtIndex(JSPropertyNameArrayRef array, size_t index)
{
    if (!array) return nullptr;

    JSStringRef ret = nullptr;

    V8_ISOLATE_CTX(CTX(array->Context()),isolate,context)
        if (index < array->L().As<Array>()->Length()) {
            Local<Value> element = array->L().As<Array>()->Get(context, index).ToLocalChecked();
            String::Utf8Value const str(element->ToString(context).ToLocalChecked());
            ret = JSStringCreateWithUTF8CString(*str);
        }
    V8_UNLOCK()

    return ret;
}

JS_EXPORT void JSPropertyNameAccumulatorAddName(JSPropertyNameAccumulatorRef accumulator,
    JSStringRef propertyName)
{
    if (accumulator && propertyName) {
        JSStringRetain(propertyName);
        accumulator->push_front(propertyName);
    }
}
