#include <cstdio>
#include <sstream>
#include <utility>
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

/**
 * load and keep data for v8::Context::FromSnapshot
 */
class SnapshotLoader {
public:
    explicit SnapshotLoader(v8::Isolate::CreateParams* createParams)
            : createParams(createParams) {}
    SnapshotLoader(const SnapshotLoader&) = delete;
    void operator=(const SnapshotLoader&) = delete;

    bool load(v8::StartupData data) {
        if (!data.IsValid()) return false;
        startupData = data;

        TimeTracker tracker;
        createParams->snapshot_blob = &startupData;
        isolate = v8::Isolate::New(*createParams);
        tracker.track("Isolate::New from startupData");
        tracker.report();
        return true;
    }

    bool load(const std::string& file) {
        if (access(file.c_str(), 0) != 0) {
            return false;
        }
        TimeTracker tracker;
        fileData = readFile(file);
        tracker.track("readFile");
        tracker.report();

        startupData = {
                fileData.data(),
                static_cast<int>(fileData.size())
        };
        if (!load(startupData)) return false;
        return true;
    }

    v8::Local<v8::Context> createContext(size_t index) const {
        return v8::Context::FromSnapshot(isolate, index).ToLocalChecked();
    }

    void freeData() {
        fileData.clear();
        startupData = {};
    }

public:
    v8::Isolate::CreateParams* createParams;
    v8::Isolate* isolate{};
    std::string fileData;
    v8::StartupData startupData{};
};

class SnapshotCreator {
public:
    SnapshotCreator() = default;
    SnapshotCreator(const SnapshotCreator&) = delete;
    void operator=(const SnapshotCreator&) = delete;

    void create(const char* js, const std::string& snapshotFile) {
        using namespace v8;
        TimeTracker tracker;
        Isolate* isolate = Isolate::Allocate();
        std::vector<intptr_t> external_references = { reinterpret_cast<intptr_t>(nullptr)};
        v8::SnapshotCreator snapshot_creator(isolate, external_references.data());
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
            LOGI("context index: %zu", index);
        }
        startupData = snapshot_creator.CreateBlob(v8::SnapshotCreator::FunctionCodeHandling::kKeep);
        tracker.track("CreateBlob");
        writeFile(startupData.data, startupData.raw_size, SNAPSHOT_FILE);
        tracker.track("writeFile");
        LOGI("size of blob: %d %.2fk", startupData.raw_size, startupData.raw_size / 1024.f);
        tracker.report();
    }

public:
    v8::StartupData startupData{};
};



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

    SnapshotLoader snapshotLoader(&create_params);
    SnapshotCreator snapshotCreator; // todo: free
    if (!snapshotLoader.load(SNAPSHOT_FILE)) {
        auto str = readFile(argv[1]);
        snapshotCreator.create(str.c_str(), SNAPSHOT_FILE);
        snapshotLoader.load(SNAPSHOT_FILE);
    }
    isolate = snapshotLoader.isolate;
    timeTracker.track("snapshotLoad total");

    {
        v8::Isolate::Scope isolate_scope(isolate);

        // Create a stack-allocated handle scope.
        v8::HandleScope handle_scope(isolate);

        // Create the Context from the snapshot index.
        context = snapshotLoader.createContext(0);
        timeTracker.track("v8::Context::FromSnapshot");
        snapshotLoader.freeData();

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

        LOGI("!!!");
        compileAndRun(context, "f(1)");
    }

    timeTracker.report();

    // Dispose the isolate and tear down V8.
    isolate->Dispose();
    v8::V8::Dispose();
    v8::V8::ShutdownPlatform();
    delete create_params.array_buffer_allocator;
    return 0;
}
