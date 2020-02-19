package v8go

// #include <stdlib.h>
// #include "v8go.h"
import "C"
import (
	"runtime"
	"unsafe"
)

// Value represents all Javascript values and objects
type Value struct {
	ptr C.ValuePtr
}

// Get a field from the object.  If this value is not an object, this will fail.
func (v *Value) Get(name string) (*Value, error) {
	name_cstr := C.CString(name)
	rtn := C.ValueGet(v.ptr, name_cstr)
	C.free(unsafe.Pointer(name_cstr))
	return getValue(rtn.value), getError(rtn.error)
}

// Set a field on the object.  If this value is not an object, this
// will fail.
func (v *Value) Set(name string, value *Value) error {
	name_cstr := C.CString(name)
	err := C.ValueSet(v.ptr, name_cstr, value.ptr)
	C.free(unsafe.Pointer(name_cstr))
	return getError(err)
}

// Call this value as a function. If v JS value is not a function, this will
// fail.
func (v *Value) Call(ctx *Context, self *Value, args ...*Value) (*Value, error) {
	// always allocate at least one so &argPtrs[0] works.
	argPtrs := make([]C.ValuePtr, len(args)+1)
	for i := range args {
		argPtrs[i] = args[i].ptr
	}

	var selfPtr C.ValuePtr
	if self != nil {
		selfPtr = self.ptr
	}

	// Perform Call
	rtn := C.ValueCall(v.ptr, selfPtr, C.int(len(args)), &argPtrs[0])

	return getValue(rtn.value), getError(rtn.error)
}

// String will return the string representation of the value. Primitive values
// are returned as-is, objects will return `[object Object]` and functions will
// print their definition.
func (v *Value) String() string {
	s := C.ValueToString(v.ptr)
	defer C.free(unsafe.Pointer(s))
	return C.GoString(s)
}

// Bool returns this Value as a boolean. If the underlying value is not a
// boolean, it will be coerced to a boolean using Javascript's coercion rules.
func (v *Value) Bool() bool {
	return C.ValueToBool(v.ptr) == 1 
}

// Int64 returns this Value as an int64. If this value is not a number,
// then 0 will be returned.
func (v *Value) Int64() int64 {
	return int64(C.ValueToInt64(v.ptr))
}

// Float64 returns this Value as a float64. If this value is not a number,
// then NaN will be returned.
func (v *Value) Float64() float64 {
	return float64(C.ValueToFloat64(v.ptr))
}

func (v *Value) finalizer() {
	C.ValueDispose(v.ptr)
	v.ptr = nil
	runtime.SetFinalizer(v, nil)
}

func getValue(rtnVal C.ValuePtr) *Value {
	if rtnVal == nil {
		return nil
	}
	v := &Value{rtnVal}
	runtime.SetFinalizer(v, (*Value).finalizer)
	return v
}

func getError(rtnErr C.RtnError) error {
	if rtnErr.msg == nil {
		return nil
	}
	err := &JSError{
		Message:    C.GoString(rtnErr.msg),
		Location:   C.GoString(rtnErr.location),
		StackTrace: C.GoString(rtnErr.stack),
	}
	C.free(unsafe.Pointer(rtnErr.msg))
	C.free(unsafe.Pointer(rtnErr.location))
	C.free(unsafe.Pointer(rtnErr.stack))
	return err
}
