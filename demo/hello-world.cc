#include <cstdio>
#include <sstream>

#include "include/libplatform/libplatform.h"
#include "include/v8.h"
#include "log.h"

std::string readFile(const std::string& filePath) {
    std::ifstream fs(filePath);
    std::ostringstream ss;
    ss << fs.rdbuf();
    return ss.str();
}

int main(int argc, char* argv[]) {
    // Initialize V8.
    v8::V8::InitializeICUDefaultLocation(argv[0]);
    v8::V8::InitializeExternalStartupData(argv[0]);
    std::unique_ptr<v8::Platform> platform = v8::platform::NewDefaultPlatform();
    v8::V8::InitializePlatform(platform.get());
    v8::V8::Initialize();

    // Create a new Isolate and make it the current one.
    v8::Isolate::CreateParams create_params;
    create_params.array_buffer_allocator =
            v8::ArrayBuffer::Allocator::NewDefaultAllocator();
    v8::Isolate* isolate = v8::Isolate::New(create_params);
    {
        v8::Isolate::Scope isolate_scope(isolate);

        // Create a stack-allocated handle scope.
        v8::HandleScope handle_scope(isolate);

        // Create a new context.
        v8::Local<v8::Context> context = v8::Context::New(isolate);

        // Enter the context for compiling and running the hello world script.
        v8::Context::Scope context_scope(context);

        {
            using namespace v8;
            auto global = context->Global();

            // 1. 绑定一个全局函数func1
            {
                v8::Local<v8::FunctionTemplate> ft = v8::FunctionTemplate::New(isolate, [](const FunctionCallbackInfo<Value>& info){
                    LOGD("func1");
                });
                v8::Local<v8::String> fnName = v8::String::NewFromUtf8(isolate, "func1").ToLocalChecked();
                v8::Local<v8::Function> func = ft->GetFunction(context).ToLocalChecked();
                func->SetName(fnName); //todo: 有什么用？
                global->Set(context, fnName, func);
            }
            // 2. js中新建对象MyObject，MyObject有个方法为func()
            {
                v8::Local<v8::String> objName = v8::String::NewFromUtf8(isolate, "MyObject").ToLocalChecked();
                v8::Local<v8::String> fnName = v8::String::NewFromUtf8(isolate, "func").ToLocalChecked();
                Handle<ObjectTemplate> objectTemplate = ObjectTemplate::New(isolate);
                objectTemplate->Set(fnName, FunctionTemplate::New(isolate, [](const FunctionCallbackInfo<Value>& info){
                    LOGD("MyObject.func()");
                }));
                v8::Local<v8::Object> obj = objectTemplate->NewInstance(context).ToLocalChecked();
                global->Set(context, objName, obj);
            }
            // 3. new MyClass()生成一个对象，这个对象里面有个"func"方法
            {
                Local<FunctionTemplate> ft = FunctionTemplate::New(isolate, [](const FunctionCallbackInfo<Value>& info){
                    LOGD("new MyClass()");
                });
                v8::Local<v8::String> className = v8::String::NewFromUtf8(isolate, "MyClass").ToLocalChecked();
                v8::Local<v8::String> fnName = v8::String::NewFromUtf8(isolate, "func").ToLocalChecked();
                //ft->SetClassName(className);
                Local<ObjectTemplate> objectTemplate = ft->InstanceTemplate();
                objectTemplate->SetInternalFieldCount(1);
                objectTemplate->Set(fnName, FunctionTemplate::New(isolate, [](const FunctionCallbackInfo<Value>& info){
                    LOGD("myClass.func()");
                }));
                global->Set(context, className, ft->GetFunction(context).ToLocalChecked());
            }
            // console.log 为了方便打印
            {
                v8::Local<v8::Object> console = v8::Object::New(isolate);
                console->Set(context, v8::String::NewFromUtf8(isolate, "log").ToLocalChecked(),
                             v8::FunctionTemplate::New(isolate, [](const v8::FunctionCallbackInfo<Value> &args) {
                                 auto isolate = args.GetIsolate();
                                 std::stringstream str;
                                 int l = args.Length();
                                 for (int i = 0; i < l; i++) {
                                     auto s = args[i]->ToString(isolate->GetCurrentContext()).ToLocalChecked();
                                     str << " " << *(v8::String::Utf8Value(isolate, s));
                                 }
                                 const std::string& tempStr = str.str();
                                 LOGI("Console: %s", tempStr.c_str());
                             })->GetFunction(context).ToLocalChecked());
                global->Set(context, v8::String::NewFromUtf8(isolate, "console").ToLocalChecked(), console);
            }
            // SetHandler: Sets a named property handler on the object template.
            {
                v8::Local<v8::ObjectTemplate> objectTpl = v8::ObjectTemplate::New(isolate);
                objectTpl->SetHandler(v8::NamedPropertyHandlerConfiguration([](Local<Name> property, const PropertyCallbackInfo<Value>& info){
                    LOGD("objectTpl Handler:");
                    v8::Isolate *isolate = info.GetIsolate();
                    v8::Local<v8::Context> context = isolate->GetCurrentContext();

                    v8::Isolate::Scope isolateScope(isolate);
                    v8::EscapableHandleScope scope(isolate);
                    v8::Context::Scope ctxScope(context);

                    v8::Local<v8::FunctionTemplate> funcTpl = v8::FunctionTemplate::New(isolate, [](const FunctionCallbackInfo<Value>& info){
                        LOGD("constructor");
                        // 这里可以注册一些方法
                    });

                    v8::Local<v8::String> nameStr = property->ToString(context).ToLocalChecked();
                    {	// log
                        auto r = nameStr->ToString(context).ToLocalChecked();
                        v8::String::Utf8Value name(isolate, r);
                        printf("name:%s\n", *name);
                    }
                    v8::Local<v8::String> type =String::NewFromUtf8(isolate, "type").ToLocalChecked();
                    funcTpl->SetClassName(nameStr);
                    funcTpl->InstanceTemplate()->Set(type, nameStr, v8::PropertyAttribute::ReadOnly);

                    v8::Local<v8::ObjectTemplate> objTpl = v8::ObjectTemplate::New(isolate);
                    objTpl->Set(v8::String::NewFromUtf8(isolate, "Instance").ToLocalChecked(), funcTpl);

                    v8::Local<v8::Object> instance = objTpl->NewInstance(context).ToLocalChecked();
                    info.GetReturnValue().Set(scope.Escape(instance));
                }));
                global->Set(context
                        , v8::String::NewFromUtf8(isolate, "BCanvas").ToLocalChecked()
                        , objectTpl->NewInstance(context).ToLocalChecked());
            }
        }

        {
            auto str = readFile(argv[1]);
            const char* csource = str.c_str();
            // Create a string containing the JavaScript source code.
            v8::Local<v8::String> source = v8::String::NewFromUtf8(isolate, csource).ToLocalChecked();

            // Compile the source code.
            v8::Local<v8::Script> script = v8::Script::Compile(context, source).ToLocalChecked();

            // Run the script to get the result.
            v8::Local<v8::Value> result = script->Run(context).ToLocalChecked();

            // Print result
            auto r = result->ToString(context).ToLocalChecked();
            v8::String::Utf8Value sss(isolate, r);
            printf("%s\n", *sss);
        }
    }

    // Dispose the isolate and tear down V8.
    isolate->Dispose();
    v8::V8::Dispose();
    v8::V8::ShutdownPlatform();
    delete create_params.array_buffer_allocator;
    return 0;
}
