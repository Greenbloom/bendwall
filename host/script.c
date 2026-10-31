/* SceneScript host. Bend has no JavaScript runtime; QuickJS runs update(). */
#include <quickjs.h>
#include <stdlib.h>
#include <string.h>

static char* dup_js(JSContext* ctx, JSValue v) {
  const char* s;
  char* out;
  size_t n;
  s = JS_ToCString(ctx, v);
  if (!s) return NULL;
  n = strlen(s);
  out = (char*)malloc(n + 1);
  if (out) memcpy(out, s, n + 1);
  JS_FreeCString(ctx, s);
  return out;
}

/* src is either a module defining update(value), or a bare body `return value;`. */
char* bw_script_update(const char* src, const char* value) {
  JSRuntime* rt;
  JSContext* ctx;
  JSValue global, fn, arg, ret;
  char* wrapped = NULL;
  char* out = NULL;
  const char* code = src ? src : "";
  if (!value) value = "";
  rt = JS_NewRuntime();
  if (!rt) return NULL;
  ctx = JS_NewContext(rt);
  if (!ctx) { JS_FreeRuntime(rt); return NULL; }
  JS_Eval(ctx, code, strlen(code), "<scenescript>", JS_EVAL_TYPE_GLOBAL);
  global = JS_GetGlobalObject(ctx);
  fn = JS_GetPropertyStr(ctx, global, "update");
  if (JS_IsUndefined(fn) || JS_IsException(fn)) {
    JS_FreeValue(ctx, fn);
    wrapped = (char*)malloc(strlen(code) + 64);
    if (!wrapped) goto done;
    sprintf(wrapped, "function update(value){ %s }", code);
    JS_Eval(ctx, wrapped, strlen(wrapped), "<wrap>", JS_EVAL_TYPE_GLOBAL);
    JS_FreeValue(ctx, global);
    global = JS_GetGlobalObject(ctx);
    fn = JS_GetPropertyStr(ctx, global, "update");
  }
  if (JS_IsFunction(ctx, fn)) {
    arg = JS_NewString(ctx, value);
    ret = JS_Call(ctx, fn, global, 1, &arg);
    if (!JS_IsException(ret)) out = dup_js(ctx, ret);
    JS_FreeValue(ctx, ret);
    JS_FreeValue(ctx, arg);
  }
  JS_FreeValue(ctx, fn);
done:
  JS_FreeValue(ctx, global);
  free(wrapped);
  JS_FreeContext(ctx);
  JS_FreeRuntime(rt);
  return out;
}
