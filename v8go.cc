#include "v8go.h"

#include "v8.h"
#include "libplatform/libplatform.h"

#include <cstdlib>
#include <cstring>
#include <string>
#include <sstream>
#include <stdio.h>

#define ISOLATE_SCOPE(iso) \
  v8::Isolate* isolate = (iso);                                                               \
  v8::Locker locker(isolate);                            /* Lock to current thread.        */ \
  v8::Isolate::Scope isolate_scope(isolate);             /* Assign isolate to this thread. */


#define CONTEXT_SCOPE(ctxptr) \
  ISOLATE_SCOPE(static_cast<m_ctx*>(ctxptr)->iso)                                       \
  v8::HandleScope handle_scope(isolate);                 /* Create a scope for handles.    */ \
  v8::Local<v8::Context> ctx(static_cast<m_ctx*>(ctxptr)->ptr.Get(isolate));                \
  v8::Context::Scope context_scope(ctx);                 /* Scope to this context.         */

auto default_platform = v8::platform::NewDefaultPlatform();
auto default_allocator = v8::ArrayBuffer::Allocator::NewDefaultAllocator();

typedef struct {
  v8::Persistent<v8::Context> ptr;
  v8::Isolate* iso;
} m_ctx;

typedef struct {
  v8::Persistent<v8::Value> ptr;
  m_ctx* ctx_ptr;
} m_value;

const char* CopyString(std::string str) {
  int len = str.length();
  char *mem = (char*)malloc(len+1);
  memcpy(mem, str.data(), len);
  mem[len] = 0;
  return mem;
}

const char* CopyString(v8::String::Utf8Value& value) {
  if (value.length() == 0) {
    return nullptr;
  }
  return CopyString(*value);
}

RtnError ExceptionError(v8::TryCatch& try_catch, v8::Isolate* iso, v8::Local<v8::Context> ctx) {
  v8::Locker locker(iso);
  v8::Isolate::Scope isolate_scope(iso);
  v8::HandleScope handle_scope(iso);

  RtnError rtn = {nullptr, nullptr, nullptr};

  if (try_catch.HasTerminated()) {
    rtn.msg = CopyString("ExecutionTerminated: script execution has been terminated");
    return rtn;
  }

  v8::String::Utf8Value exception(iso, try_catch.Exception());
  rtn.msg = CopyString(exception);

  v8::Local<v8::Message> msg = try_catch.Message();
  if (!msg.IsEmpty()) {
    v8::String::Utf8Value origin(iso, msg->GetScriptOrigin().ResourceName());
    std::ostringstream sb;
    sb << *origin;
    
    v8::Maybe<int> line = try_catch.Message()->GetLineNumber(ctx);
    if (line.IsJust()) {
      sb << ":" << line.ToChecked();
    }

    v8::Maybe<int> start = try_catch.Message()->GetStartColumn(ctx);
    if (start.IsJust()) {
      sb << ":" << start.ToChecked() + 1; // + 1 to match output from stack trace
    }

    rtn.location = CopyString(sb.str());
  }
 
  v8::MaybeLocal<v8::Value> mstack = try_catch.StackTrace(ctx);
  if (!mstack.IsEmpty()) {
    v8::String::Utf8Value stack(iso, mstack.ToLocalChecked());
    rtn.stack = CopyString(stack);
  }
  
  return rtn;
}

extern "C"
{

/********** Isolate **********/

void Init() {
    v8::V8::InitializePlatform(default_platform.get());
    v8::V8::Initialize();
    return;
}

IsolatePtr NewIsolate() {
    v8::Isolate::CreateParams params;
    params.array_buffer_allocator = default_allocator;
    return static_cast<IsolatePtr>(v8::Isolate::New(params));
}

void IsolateDispose(IsolatePtr ptr) {
    if (ptr == nullptr) {
        return;
    }
    v8::Isolate* isolate = static_cast<v8::Isolate*>(ptr);
    isolate->Dispose();
}

void IsolateTerminateExecution(IsolatePtr ptr) {
    v8::Isolate* isolate = static_cast<v8::Isolate*>(ptr);
    isolate->TerminateExecution();
}

IsolateHStatistics IsolationGetHeapStatistics(IsolatePtr ptr) {
  if (ptr == nullptr) {
    return IsolateHStatistics{0};
  }
  v8::Isolate* isolate = static_cast<v8::Isolate*>(ptr);
  v8::HeapStatistics hs;
  isolate->GetHeapStatistics(&hs);
  
  return IsolateHStatistics{
    hs.total_heap_size(),
    hs.total_heap_size_executable(),
    hs.total_physical_size(),
    hs.total_available_size(),
    hs.used_heap_size(),
    hs.heap_size_limit(),
    hs.malloced_memory(),
    hs.external_memory(),
    hs.peak_malloced_memory(),
    hs.number_of_native_contexts(),
    hs.number_of_detached_contexts()
  };
}

/********** Context **********/

ContextPtr NewContext(IsolatePtr ptr) {
    ISOLATE_SCOPE(static_cast<v8::Isolate*>(ptr));
    v8::HandleScope handle_scope(isolate);
  
    isolate->SetCaptureStackTraceForUncaughtExceptions(true);
    
    m_ctx* ctx = new m_ctx;
    ctx->ptr.Reset(isolate, v8::Context::New(isolate));
    ctx->iso = isolate;
    return static_cast<ContextPtr>(ctx);
}

RtnValue RunScript(ContextPtr ctx_ptr, const char* source, const char* origin) {
    CONTEXT_SCOPE(ctx_ptr);
    v8::TryCatch try_catch(isolate);

    v8::Local<v8::String> src = v8::String::NewFromUtf8(isolate, source, v8::NewStringType::kNormal).ToLocalChecked();
    v8::Local<v8::String> ogn = v8::String::NewFromUtf8(isolate, origin, v8::NewStringType::kNormal).ToLocalChecked();

    RtnValue rtn = { nullptr, nullptr };

    // Compile script
    v8::ScriptOrigin script_origin(ogn);
    v8::MaybeLocal<v8::Script> script = v8::Script::Compile(ctx, src, &script_origin);
    if (script.IsEmpty()) {
      rtn.error = ExceptionError(try_catch, isolate, ctx);
      return rtn;
    } 

    // Run Script
    v8::MaybeLocal<v8::Value> result = script.ToLocalChecked()->Run(ctx);
    if (result.IsEmpty()) {
      rtn.error = ExceptionError(try_catch, isolate, ctx);
      return rtn;
    }

    // Cast result
    m_value* m_val = new m_value;
    m_val->ctx_ptr = static_cast<m_ctx*>(ctx_ptr);
    m_val->ptr.Reset(isolate, v8::Persistent<v8::Value>(isolate, result.ToLocalChecked()));
    rtn.value = static_cast<ValuePtr>(m_val);

    return rtn;
}

ValuePtr Global(ContextPtr ctx_ptr) {
  CONTEXT_SCOPE(ctx_ptr);
  m_value* m_val = new m_value;
  m_val->ctx_ptr = static_cast<m_ctx*>(ctx_ptr);
  m_val->ptr.Reset(isolate, v8::Persistent<v8::Value>(isolate, ctx->Global()));
  return static_cast<ValuePtr>(m_val);
}

ValuePtr ContextCreate(ContextPtr ctx_ptr, Value val) {
  CONTEXT_SCOPE(ctx_ptr);

  m_value* m_val = new m_value;
  m_val->ctx_ptr = static_cast<m_ctx*>(ctx_ptr);

  switch (val.Type) {
    case tBOOL:  m_val->ptr.Reset(isolate, v8::Persistent<v8::Value>(isolate, v8::Boolean::New(isolate, val.Bool == 1))); break;
    case tFLOAT64:     m_val->ptr.Reset(isolate, v8::Persistent<v8::Value>(isolate, v8::Number::New(isolate, val.Float64))); break;
    case tINT64:       m_val->ptr.Reset(isolate, v8::Persistent<v8::Value>(isolate, v8::Number::New(isolate, double(val.Int64)))); break;
    case tSTRING: {
      m_val->ptr.Reset(isolate, v8::Persistent<v8::Value>(
        isolate, 
        v8::String::NewFromUtf8(isolate, val.Data, v8::NewStringType::kNormal, val.Len).ToLocalChecked()));
      break;
    }
    case tUNDEFINED:   m_val->ptr.Reset(isolate, v8::Persistent<v8::Value>(isolate, v8::Undefined(isolate))); break;
  }

  return static_cast<ValuePtr>(m_val);
}

extern RtnValue ValueGet(ValuePtr ptr, const char* field) {
  CONTEXT_SCOPE(static_cast<m_value*>(ptr)->ctx_ptr);
  v8::TryCatch try_catch(isolate);

  v8::Local<v8::Value> object_val = static_cast<m_value*>(ptr)->ptr.Get(isolate);
  if (!object_val->IsObject()) {
    return (RtnValue){nullptr, (RtnError){CopyString("Not an object"), nullptr, nullptr}};
  }
  v8::Local<v8::Object> object = object_val->ToObject(ctx).ToLocalChecked();

  // Get Value
  v8::MaybeLocal<v8::Value> value = object->Get(ctx, v8::String::NewFromUtf8(isolate, field).ToLocalChecked());

  // Cast value
  m_value* m_val = new m_value;
  m_val->ctx_ptr = static_cast<m_ctx*>(static_cast<m_value*>(ptr)->ctx_ptr);
  m_val->ptr.Reset(isolate, v8::Persistent<v8::Value>(isolate, value.ToLocalChecked()));

  return (RtnValue){static_cast<ValuePtr>(m_val), nullptr};
}

extern RtnError ValueSet(ValuePtr ptr, const char* field, ValuePtr valueptr) {
  CONTEXT_SCOPE(static_cast<m_value*>(ptr)->ctx_ptr);
  v8::TryCatch try_catch(isolate);

  // Cast object
  v8::Local<v8::Value> object_val = static_cast<m_value*>(ptr)->ptr.Get(isolate);
  if (!object_val->IsObject()) {
    return (RtnError){CopyString("Not an object"), nullptr, nullptr};
  }
  v8::Local<v8::Object> object = object_val->ToObject(ctx).ToLocalChecked();

  // Cast new value
  v8::Local<v8::Value> value = static_cast<m_value*>(valueptr)->ptr.Get(isolate);
  
  // Set Value
  v8::Maybe<bool> res = object->Set(ctx, v8::String::NewFromUtf8(isolate, field).ToLocalChecked(), value);
  if (res.IsNothing() || !res.FromJust()) {
    return ExceptionError(try_catch, isolate, ctx);
  }

  return (RtnError){nullptr, nullptr, nullptr};
}

extern RtnValue ValueCall(ValuePtr func_ptr, ValuePtr self_ptr, int argc, ValuePtr* argv_ptr) {
  CONTEXT_SCOPE(static_cast<m_value*>(func_ptr)->ctx_ptr);
  v8::TryCatch try_catch(isolate);

  RtnValue rtn = { nullptr, nullptr };

  // Cast function
  v8::Local<v8::Value> func_val = static_cast<m_value*>(func_ptr)->ptr.Get(isolate);
  if (!func_val->IsFunction()) {
    rtn.error = (RtnError){CopyString("Not a function"), nullptr, nullptr};
    return rtn;
  }
  v8::Local<v8::Function> func = v8::Local<v8::Function>::Cast(func_val);

  // Cast self
  v8::Local<v8::Value> self;
  if (self_ptr == nullptr) {
    self = ctx->Global();
  } else {
    self = static_cast<m_value*>(self_ptr)->ptr.Get(isolate);
  }

  // Cast args
  v8::Local<v8::Value>* argv = new v8::Local<v8::Value>[argc];
  for (int i = 0; i < argc; i++) {
    argv[i] = static_cast<m_value*>(argv_ptr[i])->ptr.Get(isolate);
  }

  // Execute function call
  v8::MaybeLocal<v8::Value> result = func->Call(ctx, self, argc, argv);

  delete[] argv;

  if (result.IsEmpty()) {
    rtn.error = ExceptionError(try_catch, isolate, ctx);
    return rtn;
  }

  // Cast result
  m_value* m_val = new m_value;
  m_val->ctx_ptr = static_cast<m_ctx*>(static_cast<m_value*>(func_ptr)->ctx_ptr);
  m_val->ptr.Reset(isolate, v8::Persistent<v8::Value>(isolate, result.ToLocalChecked()));
  rtn.value = static_cast<ValuePtr>(m_val);

  return rtn;
};


void ContextDispose(ContextPtr ptr) {
    if (ptr == nullptr) {
        return;
    }

    m_ctx* ctx = static_cast<m_ctx*>(ptr);
    if (ctx == nullptr) {
        return;
    }
    ctx->ptr.Reset(); 
    delete ctx; 
} 

/********** Value **********/

void ValueDispose(ValuePtr ptr) {
  delete static_cast<m_value*>(ptr);
}

const char* ValueToString(ValuePtr ptr) {
  CONTEXT_SCOPE(static_cast<m_value*>(ptr)->ctx_ptr);
  v8::Local<v8::Value> value = static_cast<m_value*>(ptr)->ptr.Get(isolate);
  v8::String::Utf8Value utf8(isolate, value);
  
  return CopyString(utf8);
} 

int ValueToBool(ValuePtr ptr) {
  CONTEXT_SCOPE(static_cast<m_value*>(ptr)->ctx_ptr);
  v8::Local<v8::Value> value = static_cast<m_value*>(ptr)->ptr.Get(isolate);
  return value->BooleanValue(isolate) ? 1 : 0;
}

int64_t ValueToInt64(ValuePtr ptr) {
  CONTEXT_SCOPE(static_cast<m_value*>(ptr)->ctx_ptr);
  v8::Local<v8::Value> value = static_cast<m_value*>(ptr)->ptr.Get(isolate);
  v8::Maybe<int64_t> val = value->IntegerValue(ctx);
  if (val.IsNothing()) {
    return 0;
  }
  return val.ToChecked();
}

double ValueToFloat64(ValuePtr ptr) {
  CONTEXT_SCOPE(static_cast<m_value*>(ptr)->ctx_ptr);
  v8::Local<v8::Value> value = static_cast<m_value*>(ptr)->ptr.Get(isolate);
  v8::Maybe<double> val = value->NumberValue(ctx);
  if (val.IsNothing()) {
    return 0;
  }
  return val.ToChecked();
}

/********** Version **********/
  
const char* Version() {
    return v8::V8::GetVersion();
}

}

