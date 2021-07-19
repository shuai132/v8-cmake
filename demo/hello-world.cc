#include <cstdio>
#include <sstream>

#include "include/libplatform/libplatform.h"
#include "include/v8.h"
#include "log.h"
#include "Time.h"

std::string readFile(const std::string& filePath) {
    std::ifstream fs(filePath);
    std::ostringstream ss;
    ss << fs.rdbuf();
    return ss.str();
}

int main(int argc, char* argv[]) {
    std::string flags = "--prof";
    v8::V8::SetFlagsFromString(flags.c_str(), flags.size());

    // Initialize V8.
    TimeTracker tracker1;
    v8::V8::InitializeICUDefaultLocation(argv[0]);
    v8::V8::InitializeExternalStartupData(argv[0]);
    std::unique_ptr<v8::Platform> platform = v8::platform::NewDefaultPlatform();
    v8::V8::InitializePlatform(platform.get());
    v8::V8::Initialize();
    tracker1.track("Initialize");
    tracker1.report();

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
        }

        {
            TimeTracker tracker;
            auto str = readFile(argv[1]);
            tracker.track("readFile");
            const char* csource = str.c_str();
            // Create a string containing the JavaScript source code.
            v8::Local<v8::String> script_source = v8::String::NewFromUtf8(isolate, csource).ToLocalChecked();
            tracker.track("NewFromUtf8");

            v8::ScriptCompiler::Source source(script_source);
            tracker.track("ScriptCompiler::Compile");
            auto unbounded_script = v8::ScriptCompiler::CompileUnboundScript(isolate, &source, v8::ScriptCompiler::kNoCompileOptions).ToLocalChecked();
            tracker.track("CreateCodeCache");
            // Compile the source code.
            v8::Local<v8::Script> script = v8::Script::Compile(context, script_source).ToLocalChecked();
            tracker.track("Compile");

            // Run the script to get the result.
            v8::Local<v8::Value> result = script->Run(context).ToLocalChecked();
            tracker.track("Run");
            tracker.report();

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
