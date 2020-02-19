package v8go_test

import (
	"testing"

	"rogchap.com/v8go"
)

func TestValueGetSet(t *testing.T) {
	t.Parallel()
	ctx, _ := v8go.NewContext(nil)
	global := ctx.Global()
	val, _ := ctx.RunScript(`"foo"`, "")
	err := global.Set("test-key", val)
	if err != nil {
		t.Errorf("Set should not error but got %v", err)
	}
	got, err := global.Get("test-key")
	if err != nil {
		t.Errorf("Get should not error but got %v", err)
	} else if got.String() != "foo" {
		t.Errorf("Get returned %v but expected %v", got.String(), "foo")
	}
}

func TestValueString(t *testing.T) {
	t.Parallel()
	ctx, _ := v8go.NewContext(nil)
	var tests = [...]struct {
		name   string
		source string
		out    string
	}{
		{"Number", `13 * 2`, "26"},
		{"String", `"string"`, "string"},
		{"Object", `let obj = {}; obj`, "[object Object]"},
		{"Function", `let fn = function(){}; fn`, "function(){}"},
	}

	for _, tt := range tests {
		tt := tt
		t.Run(tt.name, func(t *testing.T) {
			t.Parallel()
			result, _ := ctx.RunScript(tt.source, "test.js")
			str := result.String()
			if str != tt.out {
				t.Errorf("unespected result: expected %q, got %q", tt.out, str)
			}
		})
	}
}

func TestValueCall(t *testing.T) {
	ctx, _ := v8go.NewContext(nil)
	add, _ := ctx.RunScript(`((x,y)=>(x+y+this.z))`, "")
	self, _ := ctx.RunScript(`(function(){ this.z = 3; return this; })()`, "")
	one, _ := ctx.RunScript(`1`, "")
	two, _ := ctx.RunScript(`2`, "")
	res, err := add.Call(ctx, self, one, two)
	if err != nil {
		t.Fatal(err)
	} else if num := res.Int64(); num != 6 {
		t.Errorf("Expected 6, got %v (%v)", num, res)
	}
}
