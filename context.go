package v8go

// #include <stdlib.h>
// #include "v8go.h"
import "C"
import (
	"fmt"
	"reflect"
	"runtime"
	"unsafe"
)

// Context is a global root execution environment that allows separate,
// unrelated, JavaScript applications to run in a single instance of V8.
type Context struct {
	iso *Isolate
	ptr C.ContextPtr
}

// NewContext creates a new JavaScript context for a given isoltate;
// if isolate is `nil` than a new isolate will be created.
func NewContext(iso *Isolate) (*Context, error) {
	if iso == nil {
		var err error
		iso, err = NewIsolate()
		if err != nil {
			return nil, fmt.Errorf("context: failed to create new Isolate: %v", err)
		}
	}

	// TODO: [RC] does the isolate need to track all the contexts created?
	// any script run against the context should make sure the VM has not been
	// terninated
	ctx := &Context{
		iso: iso,
		ptr: C.NewContext(iso.ptr),
	}

	runtime.SetFinalizer(ctx, (*Context).finalizer)
	// TODO: [RC] catch any C++ exceptions and return as error
	return ctx, nil
}

// Isolate gets the current context's parent isolate.An  error is returned
// if the isolate has been terninated.
func (c *Context) Isolate() (*Isolate, error) {
	// TODO: [RC] check to see if the isolate has not been terninated
	return c.iso, nil
}

func (ctx *Context) Create(val interface{}) (*Value, error) {
	v, _, err := ctx.create(reflect.ValueOf(val))
	return v, err
}

var float64Type = reflect.TypeOf(float64(0))

func (ctx *Context) create(val reflect.Value) (v *Value, allocated bool, err error) {
	if !val.IsValid() {
		return ctx.createVal(C.Value{Type: C.tUNDEFINED}), true, nil
	}

	switch val.Kind() {
	case reflect.Bool:
		bval := C.int(0)
		if val.Bool() {
			bval = 1
		}
		return ctx.createVal(C.Value{Type: C.tBOOL, Bool: bval}), true, nil
	case reflect.Int, reflect.Int8, reflect.Int16, reflect.Int32, reflect.Int64,
		reflect.Uint, reflect.Uint8, reflect.Uint16, reflect.Uint32, reflect.Uint64,
		reflect.Float32, reflect.Float64:
		num := C.double(val.Convert(float64Type).Float())
		return ctx.createVal(C.Value{Type: C.tFLOAT64, Float64: num}), true, nil
	case reflect.String:
		gostr := val.String()
		str := C.CString(gostr)
		len := C.int(len(gostr))
		defer C.free(unsafe.Pointer(str))
		return ctx.createVal(C.Value{Type: C.tSTRING, Data: str, Len: len}), true, nil
	default:
		return nil, false, fmt.Errorf("Unsuported value kind %v", val.Kind())
	}
}

func (ctx *Context) createVal(v C.Value) *Value {
	return ctx.value(C.ContextCreate(ctx.ptr, v))
}

// RunScript executes the source JavaScript; origin or filename provides a
// reference for the script and used in the stack trace if there is an error.
// error will be of type `JSError` of not nil.
func (c *Context) RunScript(source string, origin string) (*Value, error) {
	cSource := C.CString(source)
	cOrigin := C.CString(origin)
	defer C.free(unsafe.Pointer(cSource))
	defer C.free(unsafe.Pointer(cOrigin))

	rtn := C.RunScript(c.ptr, cSource, cOrigin)
	return getValue(rtn.value), getError(rtn.error)
}

// Global returns the JS global object for this context, with properties like
// Object, Array, JSON, etc.
func (ctx *Context) Global() *Value {
	return ctx.value(C.Global(ctx.ptr))
}

func (ctx *Context) value(ptr C.ValuePtr) *Value {
	if ptr == nil {
		return nil
	}

	val := &Value{ptr}
	runtime.SetFinalizer(val, (*Value).finalizer)
	return val
}

// Close will dispose the context and free the memory.
func (c *Context) Close() {
	c.finalizer()
}

func (c *Context) finalizer() {
	C.ContextDispose(c.ptr)
	c.ptr = nil
	runtime.SetFinalizer(c, nil)
}
