/****************************************************************************
** Meta object code from reading C++ file 'DockModel.h'
**
** Created by: The Qt Meta Object Compiler version 69 (Qt 6.11.2)
**
** WARNING! All changes made in this file will be lost!
*****************************************************************************/

#include "../../../src/DockModel.h"
#include <QtCore/qmetatype.h>

#include <QtCore/qtmochelpers.h>

#include <memory>


#include <QtCore/qxptype_traits.h>
#if !defined(Q_MOC_OUTPUT_REVISION)
#error "The header file 'DockModel.h' doesn't include <QObject>."
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
struct qt_meta_tag_ZN9DockModelE_t {};
} // unnamed namespace

template <> constexpr inline auto DockModel::qt_create_metaobjectdata<qt_meta_tag_ZN9DockModelE_t>()
{
    namespace QMC = QtMocConstants;
    QtMocHelpers::StringRefStorage qt_stringData {
        "DockModel",
        "countChanged",
        "",
        "windowActionRequested",
        "internalId",
        "action",
        "activate",
        "row",
        "activateWindow",
        "windowIndex",
        "toggleMinimized",
        "launch",
        "close",
        "closeAll",
        "togglePinned",
        "pin",
        "unpin",
        "move",
        "from",
        "to",
        "setCustomIcon",
        "iconName",
        "clearCustomIcon",
        "pinUrl",
        "QUrl",
        "url",
        "isFolder",
        "folderEntries",
        "QVariantList",
        "openUrl",
        "panelEntryMatches",
        "panelType",
        "panelEntryPosition",
        "panelEntryCount",
        "count",
        "Role",
        "AppIdRole",
        "DesktopFileNameRole",
        "IconNameRole",
        "DisplayNameRole",
        "PinnedRole",
        "RunningRole",
        "ActiveRole",
        "MinimizedRole",
        "WindowCountRole",
        "WindowIdsRole",
        "WindowTitlesRole",
        "FolderRole"
    };

    QtMocHelpers::UintData qt_methods {
        // Signal 'countChanged'
        QtMocHelpers::SignalData<void()>(1, 2, QMC::AccessPublic, QMetaType::Void),
        // Signal 'windowActionRequested'
        QtMocHelpers::SignalData<void(const QString &, const QString &)>(3, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::QString, 4 }, { QMetaType::QString, 5 },
        }}),
        // Method 'activate'
        QtMocHelpers::MethodData<void(int)>(6, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::Int, 7 },
        }}),
        // Method 'activateWindow'
        QtMocHelpers::MethodData<void(int, int)>(8, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::Int, 7 }, { QMetaType::Int, 9 },
        }}),
        // Method 'toggleMinimized'
        QtMocHelpers::MethodData<void(int)>(10, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::Int, 7 },
        }}),
        // Method 'launch'
        QtMocHelpers::MethodData<bool(int)>(11, 2, QMC::AccessPublic, QMetaType::Bool, {{
            { QMetaType::Int, 7 },
        }}),
        // Method 'close'
        QtMocHelpers::MethodData<void(int, int)>(12, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::Int, 7 }, { QMetaType::Int, 9 },
        }}),
        // Method 'close'
        QtMocHelpers::MethodData<void(int)>(12, 2, QMC::AccessPublic | QMC::MethodCloned, QMetaType::Void, {{
            { QMetaType::Int, 7 },
        }}),
        // Method 'closeAll'
        QtMocHelpers::MethodData<void(int)>(13, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::Int, 7 },
        }}),
        // Method 'togglePinned'
        QtMocHelpers::MethodData<void(int)>(14, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::Int, 7 },
        }}),
        // Method 'pin'
        QtMocHelpers::MethodData<void(int)>(15, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::Int, 7 },
        }}),
        // Method 'unpin'
        QtMocHelpers::MethodData<void(int)>(16, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::Int, 7 },
        }}),
        // Method 'move'
        QtMocHelpers::MethodData<void(int, int)>(17, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::Int, 18 }, { QMetaType::Int, 19 },
        }}),
        // Method 'setCustomIcon'
        QtMocHelpers::MethodData<void(int, const QString &)>(20, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::Int, 7 }, { QMetaType::QString, 21 },
        }}),
        // Method 'clearCustomIcon'
        QtMocHelpers::MethodData<void(int)>(22, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::Int, 7 },
        }}),
        // Method 'pinUrl'
        QtMocHelpers::MethodData<bool(const QUrl &)>(23, 2, QMC::AccessPublic, QMetaType::Bool, {{
            { 0x80000000 | 24, 25 },
        }}),
        // Method 'isFolder'
        QtMocHelpers::MethodData<bool(int) const>(26, 2, QMC::AccessPublic, QMetaType::Bool, {{
            { QMetaType::Int, 7 },
        }}),
        // Method 'folderEntries'
        QtMocHelpers::MethodData<QVariantList(int) const>(27, 2, QMC::AccessPublic, 0x80000000 | 28, {{
            { QMetaType::Int, 7 },
        }}),
        // Method 'openUrl'
        QtMocHelpers::MethodData<bool(const QUrl &) const>(29, 2, QMC::AccessPublic, QMetaType::Bool, {{
            { 0x80000000 | 24, 25 },
        }}),
        // Method 'panelEntryMatches'
        QtMocHelpers::MethodData<bool(int, const QString &) const>(30, 2, QMC::AccessPublic, QMetaType::Bool, {{
            { QMetaType::Int, 7 }, { QMetaType::QString, 31 },
        }}),
        // Method 'panelEntryPosition'
        QtMocHelpers::MethodData<int(int, const QString &) const>(32, 2, QMC::AccessPublic, QMetaType::Int, {{
            { QMetaType::Int, 7 }, { QMetaType::QString, 31 },
        }}),
        // Method 'panelEntryCount'
        QtMocHelpers::MethodData<int(const QString &) const>(33, 2, QMC::AccessPublic, QMetaType::Int, {{
            { QMetaType::QString, 31 },
        }}),
    };
    QtMocHelpers::UintData qt_properties {
        // property 'count'
        QtMocHelpers::PropertyData<int>(34, QMetaType::Int, QMC::DefaultPropertyFlags, 0),
    };
    QtMocHelpers::UintData qt_enums {
        // enum 'Role'
        QtMocHelpers::EnumData<enum Role>(35, 35, QMC::EnumFlags{}).add({
            {   36, Role::AppIdRole },
            {   37, Role::DesktopFileNameRole },
            {   38, Role::IconNameRole },
            {   39, Role::DisplayNameRole },
            {   40, Role::PinnedRole },
            {   41, Role::RunningRole },
            {   42, Role::ActiveRole },
            {   43, Role::MinimizedRole },
            {   44, Role::WindowCountRole },
            {   45, Role::WindowIdsRole },
            {   46, Role::WindowTitlesRole },
            {   47, Role::FolderRole },
        }),
    };
    return QtMocHelpers::metaObjectData<DockModel, qt_meta_tag_ZN9DockModelE_t>(QMC::MetaObjectFlag{}, qt_stringData,
            qt_methods, qt_properties, qt_enums);
}
Q_CONSTINIT const QMetaObject DockModel::staticMetaObject = { {
    QMetaObject::SuperData::link<QAbstractListModel::staticMetaObject>(),
    qt_staticMetaObjectStaticContent<qt_meta_tag_ZN9DockModelE_t>.stringdata,
    qt_staticMetaObjectStaticContent<qt_meta_tag_ZN9DockModelE_t>.data,
    qt_static_metacall,
    nullptr,
    qt_staticMetaObjectRelocatingContent<qt_meta_tag_ZN9DockModelE_t>.metaTypes,
    nullptr
} };

void DockModel::qt_static_metacall(QObject *_o, QMetaObject::Call _c, int _id, void **_a)
{
    auto *_t = static_cast<DockModel *>(_o);
    if (_c == QMetaObject::InvokeMetaMethod) {
        switch (_id) {
        case 0: _t->countChanged(); break;
        case 1: _t->windowActionRequested((*reinterpret_cast<std::add_pointer_t<QString>>(_a[1])),(*reinterpret_cast<std::add_pointer_t<QString>>(_a[2]))); break;
        case 2: _t->activate((*reinterpret_cast<std::add_pointer_t<int>>(_a[1]))); break;
        case 3: _t->activateWindow((*reinterpret_cast<std::add_pointer_t<int>>(_a[1])),(*reinterpret_cast<std::add_pointer_t<int>>(_a[2]))); break;
        case 4: _t->toggleMinimized((*reinterpret_cast<std::add_pointer_t<int>>(_a[1]))); break;
        case 5: { bool _r = _t->launch((*reinterpret_cast<std::add_pointer_t<int>>(_a[1])));
            if (_a[0]) *reinterpret_cast<bool*>(_a[0]) = std::move(_r); }  break;
        case 6: _t->close((*reinterpret_cast<std::add_pointer_t<int>>(_a[1])),(*reinterpret_cast<std::add_pointer_t<int>>(_a[2]))); break;
        case 7: _t->close((*reinterpret_cast<std::add_pointer_t<int>>(_a[1]))); break;
        case 8: _t->closeAll((*reinterpret_cast<std::add_pointer_t<int>>(_a[1]))); break;
        case 9: _t->togglePinned((*reinterpret_cast<std::add_pointer_t<int>>(_a[1]))); break;
        case 10: _t->pin((*reinterpret_cast<std::add_pointer_t<int>>(_a[1]))); break;
        case 11: _t->unpin((*reinterpret_cast<std::add_pointer_t<int>>(_a[1]))); break;
        case 12: _t->move((*reinterpret_cast<std::add_pointer_t<int>>(_a[1])),(*reinterpret_cast<std::add_pointer_t<int>>(_a[2]))); break;
        case 13: _t->setCustomIcon((*reinterpret_cast<std::add_pointer_t<int>>(_a[1])),(*reinterpret_cast<std::add_pointer_t<QString>>(_a[2]))); break;
        case 14: _t->clearCustomIcon((*reinterpret_cast<std::add_pointer_t<int>>(_a[1]))); break;
        case 15: { bool _r = _t->pinUrl((*reinterpret_cast<std::add_pointer_t<QUrl>>(_a[1])));
            if (_a[0]) *reinterpret_cast<bool*>(_a[0]) = std::move(_r); }  break;
        case 16: { bool _r = _t->isFolder((*reinterpret_cast<std::add_pointer_t<int>>(_a[1])));
            if (_a[0]) *reinterpret_cast<bool*>(_a[0]) = std::move(_r); }  break;
        case 17: { QVariantList _r = _t->folderEntries((*reinterpret_cast<std::add_pointer_t<int>>(_a[1])));
            if (_a[0]) *reinterpret_cast<QVariantList*>(_a[0]) = std::move(_r); }  break;
        case 18: { bool _r = _t->openUrl((*reinterpret_cast<std::add_pointer_t<QUrl>>(_a[1])));
            if (_a[0]) *reinterpret_cast<bool*>(_a[0]) = std::move(_r); }  break;
        case 19: { bool _r = _t->panelEntryMatches((*reinterpret_cast<std::add_pointer_t<int>>(_a[1])),(*reinterpret_cast<std::add_pointer_t<QString>>(_a[2])));
            if (_a[0]) *reinterpret_cast<bool*>(_a[0]) = std::move(_r); }  break;
        case 20: { int _r = _t->panelEntryPosition((*reinterpret_cast<std::add_pointer_t<int>>(_a[1])),(*reinterpret_cast<std::add_pointer_t<QString>>(_a[2])));
            if (_a[0]) *reinterpret_cast<int*>(_a[0]) = std::move(_r); }  break;
        case 21: { int _r = _t->panelEntryCount((*reinterpret_cast<std::add_pointer_t<QString>>(_a[1])));
            if (_a[0]) *reinterpret_cast<int*>(_a[0]) = std::move(_r); }  break;
        default: ;
        }
    }
    if (_c == QMetaObject::IndexOfMethod) {
        if (QtMocHelpers::indexOfMethod<void (DockModel::*)()>(_a, &DockModel::countChanged, 0))
            return;
        if (QtMocHelpers::indexOfMethod<void (DockModel::*)(const QString & , const QString & )>(_a, &DockModel::windowActionRequested, 1))
            return;
    }
    if (_c == QMetaObject::ReadProperty) {
        void *_v = _a[0];
        switch (_id) {
        case 0: *reinterpret_cast<int*>(_v) = _t->rowCount(); break;
        default: break;
        }
    }
}

const QMetaObject *DockModel::metaObject() const
{
    return QObject::d_ptr->metaObject ? QObject::d_ptr->dynamicMetaObject() : &staticMetaObject;
}

void *DockModel::qt_metacast(const char *_clname)
{
    if (!_clname) return nullptr;
    if (!strcmp(_clname, qt_staticMetaObjectStaticContent<qt_meta_tag_ZN9DockModelE_t>.strings))
        return static_cast<void*>(this);
    return QAbstractListModel::qt_metacast(_clname);
}

int DockModel::qt_metacall(QMetaObject::Call _c, int _id, void **_a)
{
    _id = QAbstractListModel::qt_metacall(_c, _id, _a);
    if (_id < 0)
        return _id;
    if (_c == QMetaObject::InvokeMetaMethod) {
        if (_id < 22)
            qt_static_metacall(this, _c, _id, _a);
        _id -= 22;
    }
    if (_c == QMetaObject::RegisterMethodArgumentMetaType) {
        if (_id < 22)
            *reinterpret_cast<QMetaType *>(_a[0]) = QMetaType();
        _id -= 22;
    }
    if (_c == QMetaObject::ReadProperty || _c == QMetaObject::WriteProperty
            || _c == QMetaObject::ResetProperty || _c == QMetaObject::BindableProperty
            || _c == QMetaObject::RegisterPropertyMetaType) {
        qt_static_metacall(this, _c, _id, _a);
        _id -= 1;
    }
    return _id;
}

// SIGNAL 0
void DockModel::countChanged()
{
    QMetaObject::activate(this, &staticMetaObject, 0, nullptr);
}

// SIGNAL 1
void DockModel::windowActionRequested(const QString & _t1, const QString & _t2)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 1, nullptr, _t1, _t2);
}
QT_WARNING_POP
