package v8go_test

import (
	"encoding/json"
	"fmt"
	"testing"

	"rogchap.com/v8go"
)

func TestContextExec(t *testing.T) {
	t.Parallel()
	ctx, _ := v8go.NewContext(nil)
	_, _ = ctx.RunScript(`const add = (a, b) => a + b`, "add.js")
	val, _ := ctx.RunScript(`add(3, 4)`, "main.js")
	rtn := val.String()
	if rtn != "7" {
		t.Errorf("script returned an unexpected value: expected %q, got %q", "7", rtn)
	}

	_, err := ctx.RunScript(`add`, "func.js")
	if err != nil {
		t.Errorf("error not expected: %v", err)
	}

	iso, _ := ctx.Isolate()
	ctx2, _ := v8go.NewContext(iso)
	_, err = ctx2.RunScript(`add`, "ctx2.js")
	if err == nil {
		t.Error("error expected but was <nil>")
	}
}

func TestJSExceptions(t *testing.T) {
	t.Parallel()

	tests := [...]struct {
		name   string
		source string
		origin string
		err    string
	}{
		{"SyntaxError", "bad js syntax", "syntax.js", "SyntaxError: Unexpected identifier"},
		{"ReferenceError", "add()", "add.js", "ReferenceError: add is not defined"},
	}

	ctx, _ := v8go.NewContext(nil)
	for _, tt := range tests {
		tt := tt
		t.Run(tt.name, func(t *testing.T) {
			t.Parallel()
			_, err := ctx.RunScript(tt.source, tt.origin)
			if err == nil {
				t.Error("error expected but got <nil>")
				return
			}
			if err.Error() != tt.err {
				t.Errorf("expected %q, got %q", tt.err, err.Error())
			}
		})
	}
}

func TestCreate(t *testing.T) {
	t.Parallel()
	ctx, _ := v8go.NewContext(nil)
	val, err := ctx.Create("test-string")
	if err != nil {
		t.Errorf("Create string should not error but got %v", err)
	} else if val.String() != "test-string" {
		t.Errorf("Create string expected %q but got %q", "test-string", val.String())
	}

	val, err = ctx.Create(true)
	if err != nil {
		t.Errorf("Create boolean should not error but got %v", err)
	} else if !val.Bool() {
		t.Errorf("Create boolean expected true")
	}

	val, err = ctx.Create(10)
	if err != nil {
		t.Errorf("Create int should not error but got %v", err)
	} else if val.Int64() != int64(10) {
		t.Errorf("Create int expected 10 but got %v", val.Int64())
	}

	val, err = ctx.Create(10.1)
	if err != nil {
		t.Errorf("Create float should not error but got %v", err)
	} else if val.Float64() != float64(10.1) {
		t.Errorf("Create float expected 10.1 but got %v", val.Float64())
	}
}

func BenchmarkContext(b *testing.B) {
	b.ReportAllocs()
	vm, _ := v8go.NewIsolate()
	defer vm.Close()
	for n := 0; n < b.N; n++ {
		ctx, _ := v8go.NewContext(vm)
		_, _ = ctx.RunScript(script, "main.js")
		str, _ := json.Marshal(makeObject())
		cmd := fmt.Sprintf("process(%s)", str)
		_, _ = ctx.RunScript(cmd, "cmd.js")
		ctx.Close()
	}
}
