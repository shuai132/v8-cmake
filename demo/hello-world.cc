#include <cstdio>
#include <sstream>
#include <unistd.h>

#include "include/libplatform/libplatform.h"
#include "include/v8.h"
#include "log.h"
#include "Time.h"

#define SNAPSHOT_FILE "snapshot.bin"


std::string readFile(const std::string& filePath) {
    std::ifstream fs(filePath);
    std::ostringstream ss;
    ss << fs.rdbuf();
    return ss.str();
}

void writeFile(const void* data, ssize_t size, const std::string& filePath) {
    std::ofstream fs(filePath, std::ios::binary);
    fs.write((char*)data, size);
    fs.flush();
}

void snapshotCreate(v8::Local<v8::Context> context, const std::string& file) {
    v8::SnapshotCreator::FunctionCodeHandling function_code_handling;
    v8::SnapshotCreator snapshot_creator;
    snapshot_creator.SetDefaultContext(context);
    auto blob = snapshot_creator.CreateBlob(v8::SnapshotCreator::FunctionCodeHandling::kClear);
    writeFile(blob.data, blob.raw_size, "snapshot.blob");
}

bool snapshotLoad(v8::Isolate* isolate, const std::string& file) {
    if (access(file.c_str(), 0) != 0) {
        return false;
    }

    v8::Handle<v8::String> source_string = v8::String::NewFromUtf8(isolate, "", v8::NewStringType::kNormal).ToLocalChecked();
    auto data = readFile(file);
    auto* cache = new v8::ScriptCompiler::CachedData(reinterpret_cast<uint8_t*>(data.data()), data.size(), v8::ScriptCompiler::CachedData::BufferNotOwned);
    v8::ScriptCompiler::Source source1(source_string, cache);
    auto script = v8::ScriptCompiler::CompileUnboundScript(
            isolate, &source1, v8::ScriptCompiler::kConsumeCodeCache).ToLocalChecked();
    script->BindToCurrentContext()->Run(isolate->GetCurrentContext());
    return true;
}



bool RunExtraCode(v8::Isolate* isolate, v8::Local<v8::Context> context,
                  const char* utf8_source, const char* name) {
    v8::Context::Scope context_scope(context);
    v8::TryCatch try_catch(isolate);
    v8::Local<v8::String> source_string;
    if (!v8::String::NewFromUtf8(isolate, utf8_source, v8::NewStringType::kNormal)
            .ToLocal(&source_string)) {
        return false;
    }
    v8::Local<v8::String> resource_name =
            v8::String::NewFromUtf8(isolate, name, v8::NewStringType::kNormal)
                    .ToLocalChecked();
    v8::ScriptOrigin origin(resource_name);
    v8::ScriptCompiler::Source source(source_string, origin);
    v8::Local<v8::Script> script;
    if (!v8::ScriptCompiler::Compile(context, &source).ToLocal(&script))
        return false;
    if (script->Run(context).IsEmpty()) return false;
    //CHECK(!try_catch.HasCaught());
    return true;
}

v8::StartupData CreateSnapshotDataBlob(const char* embedded_source = nullptr) {
    // Create a new isolate and a new context from scratch, optionally run
    // a script to embed, and serialize to create a snapshot blob.
    v8::StartupData result = { nullptr, 0 };
    {
        v8::SnapshotCreator snapshot_creator;
        v8::Isolate* isolate = snapshot_creator.GetIsolate();
        {
            v8::HandleScope scope(isolate);
            v8::Local<v8::Context> context = v8::Context::New(isolate);
            if (embedded_source != nullptr &&
                !RunExtraCode(isolate, context, embedded_source, "<embedded>")) {
                return result;
            }
            snapshot_creator.SetDefaultContext(context);
        }
        result = snapshot_creator.CreateBlob(
                v8::SnapshotCreator::FunctionCodeHandling::kClear);
    }
    return result;
}


int main(int argc, char* argv[]) {
//    std::string flags = "--prof";
//    v8::V8::SetFlagsFromString(flags.c_str(), flags.size());

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

        auto compileAndRun = [&](const char* script) {
            v8::Script::Compile(context, v8::String::NewFromUtf8(isolate, script).ToLocalChecked()).ToLocalChecked()->Run(context);
        };

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

        if (!snapshotLoad(isolate, SNAPSHOT_FILE)){
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

//            CreateSnapshotDataBlob(csource);
            snapshotCreate(context, SNAPSHOT_FILE);
            // Print result
            auto r = result->ToString(context).ToLocalChecked();
            v8::String::Utf8Value sss(isolate, r);
            printf("%s\n", *sss);
            compileAndRun("f(0)");
        } else {
            compileAndRun("f(1)");
        }
    }

    // Dispose the isolate and tear down V8.
    isolate->Dispose();
    v8::V8::Dispose();
    v8::V8::ShutdownPlatform();
    delete create_params.array_buffer_allocator;
    return 0;
}
