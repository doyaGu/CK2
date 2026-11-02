#include "CKAll.h"
#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <string>

namespace {
void Require(bool ok, const char *what) { if (!ok) throw std::runtime_error(what); }
void Row(CKContext *context, const char *label) {
    std::cout << label << " high=" << context->GetObjectCount() << " list=";
    const auto &objects = context->GetObjectListByType(CKCID_OBJECT, TRUE);
    for (int i = 0; i < objects.Size(); ++i) std::cout << objects[i]->GetID() << ',';
    std::cout << '\n';
}
class Observer : public CKBaseManager {
public:
    explicit Observer(CKContext *context) : CKBaseManager(context, CKGUID(0x14D7508A, 0x4D961315), "Behavior probe") {}
    CKDWORD GetValidFunctionsMask() override {
        return CKMANAGER_FUNC_OnSequenceToBeDeleted | CKMANAGER_FUNC_OnSequenceDeleted |
               CKMANAGER_FUNC_PreClearAll | CKMANAGER_FUNC_PostClearAll;
    }
    CKERROR SequenceToBeDeleted(CK_ID *ids, int count) override { Event("before", ids, count); return CK_OK; }
    CKERROR SequenceDeleted(CK_ID *ids, int count) override { Event("after", ids, count); return CK_OK; }
    CKERROR PreClearAll() override { Row(m_Context, "pre-clear"); return CK_OK; }
    CKERROR PostClearAll() override { Row(m_Context, "post-clear"); return CK_OK; }
    void Event(const char *name, CK_ID *ids, int count) {
        std::cout << name << ':';
        for (int i = 0; i < count; ++i) {
            auto *object = m_Context->GetObject(ids[i]);
            std::cout << ids[i] << '/' << (object != nullptr) << '/' << (object && object->IsToBeDeleted() ? 1 : 0) << ',';
        }
        std::cout << '\n';
        Row(m_Context, "event-list");
    }
};
class Object : public CKObject {
public:
    bool fail = false;
    explicit Object(CKContext *context) : CKObject(context, "Probe") { context->m_ObjectManager->FinishRegisterObject(this); }
    void PreDelete() override {
        std::cout << "predelete:" << GetID() << '/' << (m_Context->GetObject(GetID()) == this) << '/' << (IsToBeDeleted() ? 1 : 0) << '\n';
        if (fail) { fail = false; throw std::runtime_error("probe"); }
        CKObject::PreDelete();
    }
};
struct Context {
    CKContext *value = nullptr;
    Context() { Require(CKCreateContext(&value, nullptr, 0, 0) == CK_OK && value, "context"); }
    ~Context() { if (value) CKCloseContext(value); }
};
void Trace() {
    Context owned;
    auto *context = owned.value;
    auto *observer = new Observer(context);
    Require(context->RegisterNewManager(observer) == CK_OK, "observer");
    auto *a = new Object(context); auto *b = new Object(context); auto *c = new Object(context);
    const auto ai = a->GetID(), bi = b->GetID(), ci = c->GetID();
    Row(context, "created");
    std::cout << "destroy-result=" << context->DestroyObject(bi, CK_DESTROY_FREEID) << '\n';
    auto *reuse = new Object(context);
    std::cout << "reuse=" << reuse->GetID() << '\n';
    CK_ID batch[] = {ai, ci};
    std::cout << "batch-result=" << context->DestroyObjects(batch, 2, CK_DESTROY_FREEID) << '\n';
    auto *first = new Object(context); auto *second = new Object(context);
    std::cout << "batch-reuse=" << first->GetID() << ',' << second->GetID() << '\n';
    const auto silent = first->GetID();
    std::cout << "silent-result=" << context->DestroyObject(silent, CK_DESTROY_NONOTIFY | CK_DESTROY_FREEID) << '\n';
    std::cout << "silent-reuse=" << (new Object(context))->GetID() << '\n';
    auto *dynamic = context->CreateObject(CKCID_OBJECT, "Dynamic", CK_OBJECTCREATION_DYNAMIC);
    Require(dynamic != nullptr, "dynamic");
    std::cout << "dynamic=" << dynamic->GetID() << '/' << (dynamic->IsDynamic() ? 1 : 0) << '\n';
    const auto pending = second->GetID();
    context->m_DeferDestroyObjects = TRUE;
    std::cout << "defer-result=" << context->DestroyObject(pending, CK_DESTROY_NONOTIFY | CK_DESTROY_FREEID) << '\n';
    std::cout << "pending=" << (context->GetObject(pending) != nullptr) << '/' << (new Object(context))->GetID() << '\n';
    context->m_DeferDestroyObjects = FALSE;
    context->m_ObjectManager->PostProcess();
    std::cout << "flushed=" << (context->GetObject(pending) != nullptr) << '\n';
    auto *copy = context->CopyObject(reuse, nullptr, " copy");
    Require(copy != nullptr, "copy");
    std::cout << "copy=" << copy->GetID() << '/' << copy->GetName() << '\n';
    Row(context, "before-clear");
    std::cout << "clear-result=" << context->ClearAll() << '\n';
    std::cout << "after-clear-id=" << (new Object(context))->GetID() << '\n';
    Row(context, "after-clear");
    {
        Context exceptional;
        auto *object = new Object(exceptional.value);
        object->fail = true;
        const auto id = object->GetID();
        try { exceptional.value->DestroyObject(id, CK_DESTROY_FREEID); }
        catch (const std::runtime_error &) { std::cout << "exception-caught\n"; }
        std::cout << "exception-object=" << (exceptional.value->GetObject(id) == object) << '/' << (object->IsToBeDeleted() ? 1 : 0) << '\n';
        Row(exceptional.value, "exception-list");
    }
}
void Files(const std::filesystem::path &directory, bool write) {
    for (const char *extension : {"nmo", "cmo", "nms"}) {
        Context owned;
        auto *context = owned.value;
        const auto path = (directory / (std::string("identity.") + extension)).string();
        if (write) {
            auto *target = context->CreateObject(CKCID_OBJECT, "Identity target");
            auto *parameter = context->CreateCKParameterOut("Identity reference", CKPGUID_OBJECT);
            Require(target && parameter, "file objects");
            const auto id = target->GetID();
            Require(parameter->SetValue(&id) == CK_OK, "set object reference");
            context->SetFileWriteMode(static_cast<CK_FILE_WRITEMODE>(0));
            CKFile file(context);
            Require(file.StartSave(path.c_str()) == CK_OK, "start save");
            file.SaveObject(target); file.SaveObject(parameter);
            Require(file.EndSave() == CK_OK, "end save");
        } else {
            for (int i = 0; i < 17; ++i) Require(context->CreateObject(CKCID_OBJECT, "Offset"), "load offset");
            CKFile file(context);
            Require(file.Load(path.c_str(), nullptr) == CK_OK, "load file");
            auto *target = context->GetObjectByName("Identity target");
            auto *parameter = CKParameter::Cast(context->GetObjectByName("Identity reference"));
            Require(target && parameter, "loaded file objects");
            CK_ID id = 0;
            Require(parameter->GetValue(&id, FALSE) == CK_OK && id == target->GetID() && id > 17, "remapped reference");
            std::cout << extension << ':' << target->GetClassID() << '/' << target->GetName() << '/' << parameter->GetName() << "/reference-remapped\n";
        }
    }
}
}
int main(int argc, char **argv) {
    try {
        Require(CKStartUp() == CK_OK, "startup");
        if (argc == 3) Files(argv[2], std::string(argv[1]) == "--write");
        else Trace();
        CKShutdown();
        return 0;
    } catch (const std::exception &error) { std::cerr << error.what() << '\n'; CKShutdown(); return 1; }
}
