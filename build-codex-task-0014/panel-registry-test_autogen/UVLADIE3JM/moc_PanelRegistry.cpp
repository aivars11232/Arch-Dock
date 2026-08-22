/****************************************************************************
** Meta object code from reading C++ file 'PanelRegistry.h'
**
** Created by: The Qt Meta Object Compiler version 69 (Qt 6.11.2)
**
** WARNING! All changes made in this file will be lost!
*****************************************************************************/

#include "../../../src/PanelRegistry.h"
#include <QtCore/qmetatype.h>

#include <QtCore/qtmochelpers.h>

#include <memory>


#include <QtCore/qxptype_traits.h>
#if !defined(Q_MOC_OUTPUT_REVISION)
#error "The header file 'PanelRegistry.h' doesn't include <QObject>."
#elif Q_MOC_OUTPUT_REVISION != 69
#error "This file was generated using the moc from 6.11.2. It"
#error "cannot be used with the include files from this version of Qt."
#error "(The moc has changed too much.)"
#endif

#ifndef Q_CONSTINIT
#define Q_CONSTINIT
#endif

QT_WARNING_PUSH
QT_WARNING_DISABLE_DEPRECATED
QT_WARNING_DISABLE_GCC("-Wuseless-cast")
namespace {
struct qt_meta_tag_ZN13PanelRegistryE_t {};
} // unnamed namespace

template <> constexpr inline auto PanelRegistry::qt_create_metaobjectdata<qt_meta_tag_ZN13PanelRegistryE_t>()
{
    namespace QMC = QtMocConstants;
    QtMocHelpers::StringRefStorage qt_stringData {
        "PanelRegistry",
        "panelsChanged",
        "",
        "nativePanelTopologyChanged",
        "activePanelIdChanged",
        "revisionChanged",
        "setActivePanelId",
        "panelId",
        "panelValue",
        "QVariant",
        "key",
        "panelName",
        "isBuiltIn",
        "setPanelValue",
        "value",
        "updatePanel",
        "QVariantMap",
        "values",
        "themeDefinitions",
        "QVariantList",
        "applyTheme",
        "themeId",
        "layer",
        "removePanel",
        "importTheme",
        "QUrl",
        "sourceUrl",
        "renderTheme",
        "width",
        "height",
        "devicePixelRatio",
        "force",
        "clearTheme",
        "panelIds",
        "activePanelId",
        "revision"
    };

    QtMocHelpers::UintData qt_methods {
        // Signal 'panelsChanged'
        QtMocHelpers::SignalData<void()>(1, 2, QMC::AccessPublic, QMetaType::Void),
        // Signal 'nativePanelTopologyChanged'
        QtMocHelpers::SignalData<void()>(3, 2, QMC::AccessPublic, QMetaType::Void),
        // Signal 'activePanelIdChanged'
        QtMocHelpers::SignalData<void()>(4, 2, QMC::AccessPublic, QMetaType::Void),
        // Signal 'revisionChanged'
        QtMocHelpers::SignalData<void()>(5, 2, QMC::AccessPublic, QMetaType::Void),
        // Slot 'setActivePanelId'
        QtMocHelpers::SlotData<void(const QString &)>(6, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::QString, 7 },
        }}),
        // Method 'panelValue'
        QtMocHelpers::MethodData<QVariant(const QString &, const QString &) const>(8, 2, QMC::AccessPublic, 0x80000000 | 9, {{
            { QMetaType::QString, 7 }, { QMetaType::QString, 10 },
        }}),
        // Method 'panelName'
        QtMocHelpers::MethodData<QString(const QString &) const>(11, 2, QMC::AccessPublic, QMetaType::QString, {{
            { QMetaType::QString, 7 },
        }}),
        // Method 'isBuiltIn'
        QtMocHelpers::MethodData<bool(const QString &) const>(12, 2, QMC::AccessPublic, QMetaType::Bool, {{
            { QMetaType::QString, 7 },
        }}),
        // Method 'setPanelValue'
        QtMocHelpers::MethodData<void(const QString &, const QString &, const QVariant &)>(13, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::QString, 7 }, { QMetaType::QString, 10 }, { 0x80000000 | 9, 14 },
        }}),
        // Method 'updatePanel'
        QtMocHelpers::MethodData<void(const QString &, const QVariantMap &)>(15, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::QString, 7 }, { 0x80000000 | 16, 17 },
        }}),
        // Method 'themeDefinitions'
        QtMocHelpers::MethodData<QVariantList() const>(18, 2, QMC::AccessPublic, 0x80000000 | 19),
        // Method 'applyTheme'
        QtMocHelpers::MethodData<bool(const QString &, const QString &, const QString &)>(20, 2, QMC::AccessPublic, QMetaType::Bool, {{
            { QMetaType::QString, 7 }, { QMetaType::QString, 21 }, { QMetaType::QString, 22 },
        }}),
        // Method 'removePanel'
        QtMocHelpers::MethodData<void(const QString &)>(23, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::QString, 7 },
        }}),
        // Method 'importTheme'
        QtMocHelpers::MethodData<bool(const QString &, const QUrl &)>(24, 2, QMC::AccessPublic, QMetaType::Bool, {{
            { QMetaType::QString, 7 }, { 0x80000000 | 25, 26 },
        }}),
        // Method 'renderTheme'
        QtMocHelpers::MethodData<bool(const QString &, int, int, qreal, bool)>(27, 2, QMC::AccessPublic, QMetaType::Bool, {{
            { QMetaType::QString, 7 }, { QMetaType::Int, 28 }, { QMetaType::Int, 29 }, { QMetaType::QReal, 30 },
            { QMetaType::Bool, 31 },
        }}),
        // Method 'renderTheme'
        QtMocHelpers::MethodData<bool(const QString &, int, int, qreal)>(27, 2, QMC::AccessPublic | QMC::MethodCloned, QMetaType::Bool, {{
            { QMetaType::QString, 7 }, { QMetaType::Int, 28 }, { QMetaType::Int, 29 }, { QMetaType::QReal, 30 },
        }}),
        // Method 'renderTheme'
        QtMocHelpers::MethodData<bool(const QString &, int, int)>(27, 2, QMC::AccessPublic | QMC::MethodCloned, QMetaType::Bool, {{
            { QMetaType::QString, 7 }, { QMetaType::Int, 28 }, { QMetaType::Int, 29 },
        }}),
        // Method 'clearTheme'
        QtMocHelpers::MethodData<void(const QString &)>(32, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::QString, 7 },
        }}),
    };
    QtMocHelpers::UintData qt_properties {
        // property 'panelIds'
        QtMocHelpers::PropertyData<QStringList>(33, QMetaType::QStringList, QMC::DefaultPropertyFlags, 0),
        // property 'activePanelId'
        QtMocHelpers::PropertyData<QString>(34, QMetaType::QString, QMC::DefaultPropertyFlags | QMC::Writable | QMC::StdCppSet, 2),
        // property 'revision'
        QtMocHelpers::PropertyData<int>(35, QMetaType::Int, QMC::DefaultPropertyFlags, 3),
    };
    QtMocHelpers::UintData qt_enums {
    };
    return QtMocHelpers::metaObjectData<PanelRegistry, qt_meta_tag_ZN13PanelRegistryE_t>(QMC::MetaObjectFlag{}, qt_stringData,
            qt_methods, qt_properties, qt_enums);
}
Q_CONSTINIT const QMetaObject PanelRegistry::staticMetaObject = { {
    QMetaObject::SuperData::link<QObject::staticMetaObject>(),
    qt_staticMetaObjectStaticContent<qt_meta_tag_ZN13PanelRegistryE_t>.stringdata,
    qt_staticMetaObjectStaticContent<qt_meta_tag_ZN13PanelRegistryE_t>.data,
    qt_static_metacall,
    nullptr,
    qt_staticMetaObjectRelocatingContent<qt_meta_tag_ZN13PanelRegistryE_t>.metaTypes,
    nullptr
} };

void PanelRegistry::qt_static_metacall(QObject *_o, QMetaObject::Call _c, int _id, void **_a)
{
    auto *_t = static_cast<PanelRegistry *>(_o);
    if (_c == QMetaObject::InvokeMetaMethod) {
        switch (_id) {
        case 0: _t->panelsChanged(); break;
        case 1: _t->nativePanelTopologyChanged(); break;
        case 2: _t->activePanelIdChanged(); break;
        case 3: _t->revisionChanged(); break;
        case 4: _t->setActivePanelId((*reinterpret_cast<std::add_pointer_t<QString>>(_a[1]))); break;
        case 5: { QVariant _r = _t->panelValue((*reinterpret_cast<std::add_pointer_t<QString>>(_a[1])),(*reinterpret_cast<std::add_pointer_t<QString>>(_a[2])));
            if (_a[0]) *reinterpret_cast<QVariant*>(_a[0]) = std::move(_r); }  break;
        case 6: { QString _r = _t->panelName((*reinterpret_cast<std::add_pointer_t<QString>>(_a[1])));
            if (_a[0]) *reinterpret_cast<QString*>(_a[0]) = std::move(_r); }  break;
        case 7: { bool _r = _t->isBuiltIn((*reinterpret_cast<std::add_pointer_t<QString>>(_a[1])));
            if (_a[0]) *reinterpret_cast<bool*>(_a[0]) = std::move(_r); }  break;
        case 8: _t->setPanelValue((*reinterpret_cast<std::add_pointer_t<QString>>(_a[1])),(*reinterpret_cast<std::add_pointer_t<QString>>(_a[2])),(*reinterpret_cast<std::add_pointer_t<QVariant>>(_a[3]))); break;
        case 9: _t->updatePanel((*reinterpret_cast<std::add_pointer_t<QString>>(_a[1])),(*reinterpret_cast<std::add_pointer_t<QVariantMap>>(_a[2]))); break;
        case 10: { QVariantList _r = _t->themeDefinitions();
            if (_a[0]) *reinterpret_cast<QVariantList*>(_a[0]) = std::move(_r); }  break;
        case 11: { bool _r = _t->applyTheme((*reinterpret_cast<std::add_pointer_t<QString>>(_a[1])),(*reinterpret_cast<std::add_pointer_t<QString>>(_a[2])),(*reinterpret_cast<std::add_pointer_t<QString>>(_a[3])));
            if (_a[0]) *reinterpret_cast<bool*>(_a[0]) = std::move(_r); }  break;
        case 12: _t->removePanel((*reinterpret_cast<std::add_pointer_t<QString>>(_a[1]))); break;
        case 13: { bool _r = _t->importTheme((*reinterpret_cast<std::add_pointer_t<QString>>(_a[1])),(*reinterpret_cast<std::add_pointer_t<QUrl>>(_a[2])));
            if (_a[0]) *reinterpret_cast<bool*>(_a[0]) = std::move(_r); }  break;
        case 14: { bool _r = _t->renderTheme((*reinterpret_cast<std::add_pointer_t<QString>>(_a[1])),(*reinterpret_cast<std::add_pointer_t<int>>(_a[2])),(*reinterpret_cast<std::add_pointer_t<int>>(_a[3])),(*reinterpret_cast<std::add_pointer_t<qreal>>(_a[4])),(*reinterpret_cast<std::add_pointer_t<bool>>(_a[5])));
            if (_a[0]) *reinterpret_cast<bool*>(_a[0]) = std::move(_r); }  break;
        case 15: { bool _r = _t->renderTheme((*reinterpret_cast<std::add_pointer_t<QString>>(_a[1])),(*reinterpret_cast<std::add_pointer_t<int>>(_a[2])),(*reinterpret_cast<std::add_pointer_t<int>>(_a[3])),(*reinterpret_cast<std::add_pointer_t<qreal>>(_a[4])));
            if (_a[0]) *reinterpret_cast<bool*>(_a[0]) = std::move(_r); }  break;
        case 16: { bool _r = _t->renderTheme((*reinterpret_cast<std::add_pointer_t<QString>>(_a[1])),(*reinterpret_cast<std::add_pointer_t<int>>(_a[2])),(*reinterpret_cast<std::add_pointer_t<int>>(_a[3])));
            if (_a[0]) *reinterpret_cast<bool*>(_a[0]) = std::move(_r); }  break;
        case 17: _t->clearTheme((*reinterpret_cast<std::add_pointer_t<QString>>(_a[1]))); break;
        default: ;
        }
    }
    if (_c == QMetaObject::IndexOfMethod) {
        if (QtMocHelpers::indexOfMethod<void (PanelRegistry::*)()>(_a, &PanelRegistry::panelsChanged, 0))
            return;
        if (QtMocHelpers::indexOfMethod<void (PanelRegistry::*)()>(_a, &PanelRegistry::nativePanelTopologyChanged, 1))
            return;
        if (QtMocHelpers::indexOfMethod<void (PanelRegistry::*)()>(_a, &PanelRegistry::activePanelIdChanged, 2))
            return;
        if (QtMocHelpers::indexOfMethod<void (PanelRegistry::*)()>(_a, &PanelRegistry::revisionChanged, 3))
            return;
    }
    if (_c == QMetaObject::ReadProperty) {
        void *_v = _a[0];
        switch (_id) {
        case 0: *reinterpret_cast<QStringList*>(_v) = _t->panelIds(); break;
        case 1: *reinterpret_cast<QString*>(_v) = _t->activePanelId(); break;
        case 2: *reinterpret_cast<int*>(_v) = _t->revision(); break;
        default: break;
        }
    }
    if (_c == QMetaObject::WriteProperty) {
        void *_v = _a[0];
        switch (_id) {
        case 1: _t->setActivePanelId(*reinterpret_cast<QString*>(_v)); break;
        default: break;
        }
    }
}

const QMetaObject *PanelRegistry::metaObject() const
{
    return QObject::d_ptr->metaObject ? QObject::d_ptr->dynamicMetaObject() : &staticMetaObject;
}

void *PanelRegistry::qt_metacast(const char *_clname)
{
    if (!_clname) return nullptr;
    if (!strcmp(_clname, qt_staticMetaObjectStaticContent<qt_meta_tag_ZN13PanelRegistryE_t>.strings))
        return static_cast<void*>(this);
    return QObject::qt_metacast(_clname);
}

int PanelRegistry::qt_metacall(QMetaObject::Call _c, int _id, void **_a)
{
    _id = QObject::qt_metacall(_c, _id, _a);
    if (_id < 0)
        return _id;
    if (_c == QMetaObject::InvokeMetaMethod) {
        if (_id < 18)
            qt_static_metacall(this, _c, _id, _a);
        _id -= 18;
    }
    if (_c == QMetaObject::RegisterMethodArgumentMetaType) {
        if (_id < 18)
            *reinterpret_cast<QMetaType *>(_a[0]) = QMetaType();
        _id -= 18;
    }
    if (_c == QMetaObject::ReadProperty || _c == QMetaObject::WriteProperty
            || _c == QMetaObject::ResetProperty || _c == QMetaObject::BindableProperty
            || _c == QMetaObject::RegisterPropertyMetaType) {
        qt_static_metacall(this, _c, _id, _a);
        _id -= 3;
    }
    return _id;
}

// SIGNAL 0
void PanelRegistry::panelsChanged()
{
    QMetaObject::activate(this, &staticMetaObject, 0, nullptr);
}

// SIGNAL 1
void PanelRegistry::nativePanelTopologyChanged()
{
    QMetaObject::activate(this, &staticMetaObject, 1, nullptr);
}

// SIGNAL 2
void PanelRegistry::activePanelIdChanged()
{
    QMetaObject::activate(this, &staticMetaObject, 2, nullptr);
}

// SIGNAL 3
void PanelRegistry::revisionChanged()
{
    QMetaObject::activate(this, &staticMetaObject, 3, nullptr);
}
QT_WARNING_POP
