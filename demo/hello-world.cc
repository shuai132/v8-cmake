#include <cstdio>
#include <sstream>

#include "include/libplatform/libplatform.h"
#include "include/v8.h"

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
