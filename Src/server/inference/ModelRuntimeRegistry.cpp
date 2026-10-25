#include "ModelRuntimeRegistry.h"

QJsonObject ModelRuntimeRegistry::Status() const
{
    return {{"mode", "mock-only"},
            {"focus", QJsonObject{{"state", m_focusMockLoaded ? "MOCK_LOADED" : "UNLOADED"},
                                  {"checkpoint_read", false}, {"inference_enabled", false}}},
            {"fau", QJsonObject{{"state", m_fauMockLoaded ? "MOCK_LOADED" : "UNLOADED"},
                                {"checkpoint_read", false}, {"inference_enabled", false}}}};
}

bool ModelRuntimeRegistry::SetMockLoaded(const QString &kind, bool loaded)
{
    if (kind == "focus") m_focusMockLoaded = loaded;
    else if (kind == "fau") m_fauMockLoaded = loaded;
    else return false;
    return true;
}
