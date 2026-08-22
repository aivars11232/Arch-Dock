#include <QtQml/qqmlprivate.h>
#include <QtCore/qdir.h>
#include <QtCore/qurl.h>
#include <QtCore/qhash.h>
#include <QtCore/qstring.h>

namespace QmlCacheGeneratedCode {
namespace _qt_qml_ArchDock_qml_runtime_DockGeometry_js { 
    extern const unsigned char qmlData[];
    extern const QQmlPrivate::AOTCompiledFunction aotBuiltFunctions[];
    const QQmlPrivate::CachedQmlUnit unit = {
        reinterpret_cast<const QV4::CompiledData::Unit*>(&qmlData), &aotBuiltFunctions[0], nullptr
    };
}
namespace _qt_qml_ArchDock_qml_runtime_FreePanelWindow_qml { 
    extern const unsigned char qmlData[];
    extern const QQmlPrivate::AOTCompiledFunction aotBuiltFunctions[];
    const QQmlPrivate::CachedQmlUnit unit = {
        reinterpret_cast<const QV4::CompiledData::Unit*>(&qmlData), &aotBuiltFunctions[0], nullptr
    };
}
namespace _qt_qml_ArchDock_qml_runtime_IconProperties_qml { 
    extern const unsigned char qmlData[];
    extern const QQmlPrivate::AOTCompiledFunction aotBuiltFunctions[];
    const QQmlPrivate::CachedQmlUnit unit = {
        reinterpret_cast<const QV4::CompiledData::Unit*>(&qmlData), &aotBuiltFunctions[0], nullptr
    };
}
namespace _qt_qml_ArchDock_qml_runtime_IconPropertiesWindow_qml { 
    extern const unsigned char qmlData[];
    extern const QQmlPrivate::AOTCompiledFunction aotBuiltFunctions[];
    const QQmlPrivate::CachedQmlUnit unit = {
        reinterpret_cast<const QV4::CompiledData::Unit*>(&qmlData), &aotBuiltFunctions[0], nullptr
    };
}
namespace _qt_qml_ArchDock_qml_runtime_StudioDraft_js { 
    extern const unsigned char qmlData[];
    extern const QQmlPrivate::AOTCompiledFunction aotBuiltFunctions[];
    const QQmlPrivate::CachedQmlUnit unit = {
        reinterpret_cast<const QV4::CompiledData::Unit*>(&qmlData), &aotBuiltFunctions[0], nullptr
    };
}
namespace _qt_qml_ArchDock_qml_runtime_StudioForm_qml { 
    extern const unsigned char qmlData[];
    extern const QQmlPrivate::AOTCompiledFunction aotBuiltFunctions[];
    const QQmlPrivate::CachedQmlUnit unit = {
        reinterpret_cast<const QV4::CompiledData::Unit*>(&qmlData), &aotBuiltFunctions[0], nullptr
    };
}
namespace _qt_qml_ArchDock_qml_runtime_StudioNavigation_js { 
    extern const unsigned char qmlData[];
    extern const QQmlPrivate::AOTCompiledFunction aotBuiltFunctions[];
    const QQmlPrivate::CachedQmlUnit unit = {
        reinterpret_cast<const QV4::CompiledData::Unit*>(&qmlData), &aotBuiltFunctions[0], nullptr
    };
}
namespace _qt_qml_ArchDock_qml_runtime_SettingsPopup_qml { 
    extern const unsigned char qmlData[];
    extern const QQmlPrivate::AOTCompiledFunction aotBuiltFunctions[];
    const QQmlPrivate::CachedQmlUnit unit = {
        reinterpret_cast<const QV4::CompiledData::Unit*>(&qmlData), &aotBuiltFunctions[0], nullptr
    };
}

}
namespace {
struct Registry {
    Registry();
    ~Registry();
    QHash<QString, const QQmlPrivate::CachedQmlUnit*> resourcePathToCachedUnit;
    static const QQmlPrivate::CachedQmlUnit *lookupCachedUnit(const QUrl &url);
};

Q_GLOBAL_STATIC(Registry, unitRegistry)


Registry::Registry() {
    resourcePathToCachedUnit.insert(QStringLiteral("/qt/qml/ArchDock/qml/runtime/DockGeometry.js"), &QmlCacheGeneratedCode::_qt_qml_ArchDock_qml_runtime_DockGeometry_js::unit);
    resourcePathToCachedUnit.insert(QStringLiteral("/qt/qml/ArchDock/qml/runtime/FreePanelWindow.qml"), &QmlCacheGeneratedCode::_qt_qml_ArchDock_qml_runtime_FreePanelWindow_qml::unit);
    resourcePathToCachedUnit.insert(QStringLiteral("/qt/qml/ArchDock/qml/runtime/IconProperties.qml"), &QmlCacheGeneratedCode::_qt_qml_ArchDock_qml_runtime_IconProperties_qml::unit);
    resourcePathToCachedUnit.insert(QStringLiteral("/qt/qml/ArchDock/qml/runtime/IconPropertiesWindow.qml"), &QmlCacheGeneratedCode::_qt_qml_ArchDock_qml_runtime_IconPropertiesWindow_qml::unit);
    resourcePathToCachedUnit.insert(QStringLiteral("/qt/qml/ArchDock/qml/runtime/StudioDraft.js"), &QmlCacheGeneratedCode::_qt_qml_ArchDock_qml_runtime_StudioDraft_js::unit);
    resourcePathToCachedUnit.insert(QStringLiteral("/qt/qml/ArchDock/qml/runtime/StudioForm.qml"), &QmlCacheGeneratedCode::_qt_qml_ArchDock_qml_runtime_StudioForm_qml::unit);
    resourcePathToCachedUnit.insert(QStringLiteral("/qt/qml/ArchDock/qml/runtime/StudioNavigation.js"), &QmlCacheGeneratedCode::_qt_qml_ArchDock_qml_runtime_StudioNavigation_js::unit);
    resourcePathToCachedUnit.insert(QStringLiteral("/qt/qml/ArchDock/qml/runtime/SettingsPopup.qml"), &QmlCacheGeneratedCode::_qt_qml_ArchDock_qml_runtime_SettingsPopup_qml::unit);
    QQmlPrivate::RegisterQmlUnitCacheHook registration;
    registration.structVersion = 0;
    registration.lookupCachedQmlUnit = &lookupCachedUnit;
    QQmlPrivate::qmlregister(QQmlPrivate::QmlUnitCacheHookRegistration, &registration);
}

Registry::~Registry() {
    QQmlPrivate::qmlunregister(QQmlPrivate::QmlUnitCacheHookRegistration, quintptr(&lookupCachedUnit));
}

const QQmlPrivate::CachedQmlUnit *Registry::lookupCachedUnit(const QUrl &url) {
    if (url.scheme() != QLatin1String("qrc"))
        return nullptr;
    QString resourcePath = QDir::cleanPath(url.path());
    if (resourcePath.isEmpty())
        return nullptr;
    if (!resourcePath.startsWith(QLatin1Char('/')))
        resourcePath.prepend(QLatin1Char('/'));
    return unitRegistry()->resourcePathToCachedUnit.value(resourcePath, nullptr);
}
}
int QT_MANGLE_NAMESPACE(qInitResources_qmlcache_arch_dock)() {
    ::unitRegistry();
    return 1;
}
Q_CONSTRUCTOR_FUNCTION(QT_MANGLE_NAMESPACE(qInitResources_qmlcache_arch_dock))
int QT_MANGLE_NAMESPACE(qCleanupResources_qmlcache_arch_dock)() {
    return 1;
}
