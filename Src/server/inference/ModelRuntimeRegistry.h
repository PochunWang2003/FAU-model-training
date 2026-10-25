#ifndef FAU_MODEL_RUNTIME_REGISTRY_H
#define FAU_MODEL_RUNTIME_REGISTRY_H

#include <QJsonObject>
#include <QString>

class ModelRuntimeRegistry final
{
public:
    QJsonObject Status() const;
    bool SetMockLoaded(const QString &kind, bool loaded);

private:
    bool m_focusMockLoaded = false;
    bool m_fauMockLoaded = false;
};

#endif
