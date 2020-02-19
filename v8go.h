#ifndef V8GO_H
#define V8GO_H
#ifdef __cplusplus
extern "C" {
#endif

#include <stddef.h>
#include <stdint.h>

typedef void* IsolatePtr;
typedef void* ContextPtr;
typedef void* ValuePtr;

typedef struct {
  const char* msg;
  const char* location;
  const char* stack;
} RtnError;

typedef struct {
  ValuePtr value;
  RtnError error;
} RtnValue;

typedef struct {
    size_t total_heap_size;
    size_t total_heap_size_executable;
    size_t total_physical_size;
    size_t total_available_size;
    size_t used_heap_size;
    size_t heap_size_limit;
    size_t malloced_memory;
    size_t external_memory;
    size_t peak_malloced_memory;
    size_t number_of_native_contexts;
    size_t number_of_detached_contexts;
} IsolateHStatistics;

typedef enum {
    tSTRING,
    tBOOL,
    tFLOAT64,
    tINT64,
    tUNDEFINED,
} ValueType;

typedef struct {
    ValueType Type;
    const char* Data;
    int Len;
    int Bool;
    double Float64;
    int64_t Int64;
} Value;

extern void Init();
extern IsolatePtr NewIsolate();
extern void IsolateDispose(IsolatePtr ptr);
extern void IsolateTerminateExecution(IsolatePtr ptr);
extern IsolateHStatistics IsolationGetHeapStatistics(IsolatePtr ptr);

extern ContextPtr NewContext(IsolatePtr prt);
extern ValuePtr Global(ContextPtr prt);
extern ValuePtr ContextCreate(ContextPtr ctx, Value val);
extern void ContextDispose(ContextPtr ptr);
extern RtnValue RunScript(ContextPtr ctx_ptr, const char* source, const char* origin);

extern RtnValue ValueCall(ValuePtr func, ValuePtr self_ptr, int argc, ValuePtr* argv);
extern RtnValue ValueGet(ValuePtr ptr, const char* field);
extern RtnError ValueSet(ValuePtr ptr, const char* field, ValuePtr valueptr);
extern void ValueDispose(ValuePtr ptr);
const char* ValueToString(ValuePtr ptr);
extern int ValueToBool(ValuePtr ptr);
extern int64_t ValueToInt64(ValuePtr ptr);
extern double ValueToFloat64(ValuePtr ptr);

const char* Version();

#ifdef __cplusplus
}  // extern "C"
#endif
#endif  // V8GO_H
