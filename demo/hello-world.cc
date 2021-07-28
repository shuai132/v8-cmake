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

static void compileAndRun(v8::Local<v8::Context> context, const char* script) {
    v8::Script::Compile(context, v8::String::NewFromUtf8(context->GetIsolate(), script).ToLocalChecked()).ToLocalChecked()->Run(context);
}

void snapshotCreate(const char* js, const std::string& snapshotFile) {
    using namespace v8;
    TimeTracker tracker;
    v8::SnapshotCreator snapshot_creator;
    Isolate* isolate = snapshot_creator.GetIsolate();
    tracker.track("snapshot_creator");
    {
        HandleScope scope(isolate);
        snapshot_creator.SetDefaultContext(Context::New(isolate));
        Local<Context> context = Context::New(isolate);
        {
            Context::Scope context_scope(context);
            TryCatch try_catch(isolate);
            compileAndRun(context, js);
            assert(!try_catch.HasCaught());
        }
        auto index = snapshot_creator.AddContext(context);
        tracker.track("Compile & Run");
        printf("context index: %zu\n", index);
    }
    StartupData blob = snapshot_creator.CreateBlob(v8::SnapshotCreator::FunctionCodeHandling::kKeep);
    tracker.track("CreateBlob");
    writeFile(blob.data, blob.raw_size, SNAPSHOT_FILE);
    tracker.track("writeFile");
    printf("size of blob: %d\n", blob.raw_size);
    tracker.report();
}

/**
 * load and keep data for v8::Context::FromSnapshot
 */
class StartupDataLoader {
public:
    void load(const std::string& file) {
        fileData = readFile(file);
        startupData = {
                fileData.data(),
                static_cast<int>(fileData.size())
        };
    }
    void freeData() {
        fileData.clear();
        startupData = {};
    }
    StartupDataLoader() = default;
    StartupDataLoader(const StartupDataLoader&) = delete;
    void operator=(const StartupDataLoader&) = delete;

    std::string fileData;
    v8::StartupData startupData{};
};

bool snapshotLoad(v8::Isolate::CreateParams& create_params, v8::Isolate*& isolate, v8::Local<v8::Context>& context, const std::string& file, StartupDataLoader& loader) {
    if (access(file.c_str(), 0) != 0) {
        return false;
    }

    TimeTracker tracker;
    loader.load(file);
    tracker.track("readFile");

    create_params.snapshot_blob = &loader.startupData;
    isolate = v8::Isolate::New(create_params);
    tracker.track("Isolate::New from startup_data");
    tracker.report();
    return true;
}

int main(int argc, char* argv[]) {
    // Initialize V8.
    TimeTracker timeTracker;
    v8::V8::InitializeICUDefaultLocation(argv[0]);
    v8::V8::InitializeExternalStartupData(argv[0]);
    std::unique_ptr<v8::Platform> platform = v8::platform::NewDefaultPlatform();
    v8::V8::InitializePlatform(platform.get());
    v8::V8::Initialize();
    timeTracker.track("Initialize");


    v8::Isolate::CreateParams create_params;
    create_params.array_buffer_allocator = v8::ArrayBuffer::Allocator::NewDefaultAllocator();
    v8::Isolate* isolate;
    v8::Local<v8::Context> context;

    StartupDataLoader startupDataLoader;
    if (!snapshotLoad(create_params, isolate, context, SNAPSHOT_FILE, startupDataLoader)) {
        TimeTracker tracker;
        auto str = readFile(argv[1]);
        tracker.track("readFile");
        snapshotCreate(str.c_str(), SNAPSHOT_FILE);
        assert(snapshotLoad(create_params, isolate, context, SNAPSHOT_FILE, startupDataLoader));
    }

    {
        v8::Isolate::Scope isolate_scope(isolate);

        // Create a stack-allocated handle scope.
        v8::HandleScope handle_scope(isolate);

        // Create the Context from the snapshot index.
        context = v8::Context::FromSnapshot(isolate, 0).ToLocalChecked();
        startupDataLoader.freeData();
        v8::Context::Scope context_scope(context);

        // 注入native能力
        {
            auto global = context->Global();
            v8::Local<v8::Object> console = v8::Object::New(isolate);
            console->Set(context, v8::String::NewFromUtf8(isolate, "log").ToLocalChecked(),
                         v8::FunctionTemplate::New(isolate, [](const v8::FunctionCallbackInfo<v8::Value> &args) {
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

        compileAndRun(context, "f(1)");
    }

    // Dispose the isolate and tear down V8.
    isolate->Dispose();
    v8::V8::Dispose();
    v8::V8::ShutdownPlatform();
    delete create_params.array_buffer_allocator;
    return 0;
}
