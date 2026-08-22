/****************************************************************************
** Meta object code from reading C++ file 'PanelWindow.h'
**
** Created by: The Qt Meta Object Compiler version 69 (Qt 6.11.2)
**
** WARNING! All changes made in this file will be lost!
*****************************************************************************/

#include "../../../src/panel/PanelWindow.h"
#include <QtCore/qmetatype.h>

#include <QtCore/qtmochelpers.h>

#include <memory>


#include <QtCore/qxptype_traits.h>
#if !defined(Q_MOC_OUTPUT_REVISION)
#error "The header file 'PanelWindow.h' doesn't include <QObject>."
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
struct qt_meta_tag_ZN11PanelWindowE_t {};
} // unnamed namespace

template <> constexpr inline auto PanelWindow::qt_create_metaobjectdata<qt_meta_tag_ZN11PanelWindowE_t>()
{
    namespace QMC = QtMocConstants;
    QtMocHelpers::StringRefStorage qt_stringData {
        "PanelWindow",
        "screenRevisionChanged",
        "",
        "visibilityRevisionChanged",
        "dockRevisionChanged",
        "dockEntriesRevisionChanged",
        "nativePanelRecoveryFinished",
        "showSettings",
        "showPanelSettings",
        "panelId",
        "createNativePanel",
        "edge",
        "type",
        "createFreePanel",
        "QVariantMap",
        "createFreePanelFromTemplate",
        "containmentId",
        "ownershipToken",
        "adoptFreePanelApplet",
        "desktopContainmentId",
        "dockAppletId",
        "saveFreePanelPosition",
        "x",
        "y",
        "setNativePanelType",
        "dockConfiguration",
        "setDockStringConfiguration",
        "key",
        "value",
        "setDockIntegerConfiguration",
        "setDockRealConfiguration",
        "setDockBooleanConfiguration",
        "dockEntries",
        "QVariantList",
        "panelType",
        "dockEntriesForPanel",
        "activateDockEntry",
        "appId",
        "activateDockWindow",
        "windowId",
        "minimizeDockEntry",
        "closeDockEntry",
        "closeAllDockEntry",
        "togglePinnedDockEntry",
        "moveDockEntryBefore",
        "beforeAppId",
        "pinDockUrl",
        "url",
        "pinDockUrls",
        "urls",
        "pinPanelUrls",
        "removePanelContent",
        "entryId",
        "dockFolderEntries",
        "openDockUrl",
        "availableKdeWidgets",
        "createNativeKdePanel",
        "addKdeWidget",
        "appletId",
        "removeNativeKdePanel",
        "removePanel",
        "openKdeWidgetPreview",
        "availableScreens",
        "screenIndexForPanel",
        "setPanelScreen",
        "screenIndex",
        "setPanelVisible",
        "visible",
        "setPanelVisibilityMode",
        "visibilityMode",
        "shouldConcealPanel",
        "resetSettings",
        "toggleAutoHide",
        "toggleDesktopSuite",
        "toggleTopLauncher",
        "toggleSideRail",
        "toggleBottomPanel",
        "applyProfile",
        "profileName",
        "openSystemSettings",
        "module",
        "showIconProperties",
        "row",
        "screenRevision",
        "visibilityRevision",
        "dockRevision",
        "dockEntriesRevision"
    };

    QtMocHelpers::UintData qt_methods {
        // Signal 'screenRevisionChanged'
        QtMocHelpers::SignalData<void()>(1, 2, QMC::AccessPublic, QMetaType::Void),
        // Signal 'visibilityRevisionChanged'
        QtMocHelpers::SignalData<void()>(3, 2, QMC::AccessPublic, QMetaType::Void),
        // Signal 'dockRevisionChanged'
        QtMocHelpers::SignalData<void()>(4, 2, QMC::AccessPublic, QMetaType::Void),
        // Signal 'dockEntriesRevisionChanged'
        QtMocHelpers::SignalData<void()>(5, 2, QMC::AccessPublic, QMetaType::Void),
        // Signal 'nativePanelRecoveryFinished'
        QtMocHelpers::SignalData<void()>(6, 2, QMC::AccessPublic, QMetaType::Void),
        // Slot 'showSettings'
        QtMocHelpers::SlotData<void()>(7, 2, QMC::AccessPublic, QMetaType::Void),
        // Slot 'showPanelSettings'
        QtMocHelpers::SlotData<void(const QString &)>(8, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::QString, 9 },
        }}),
        // Slot 'createNativePanel'
        QtMocHelpers::SlotData<QString(const QString &, const QString &)>(10, 2, QMC::AccessPublic, QMetaType::QString, {{
            { QMetaType::QString, 11 }, { QMetaType::QString, 12 },
        }}),
        // Slot 'createFreePanel'
        QtMocHelpers::SlotData<QVariantMap()>(13, 2, QMC::AccessPublic, 0x80000000 | 14),
        // Slot 'createFreePanelFromTemplate'
        QtMocHelpers::SlotData<QVariantMap(int, const QString &)>(15, 2, QMC::AccessPublic, 0x80000000 | 14, {{
            { QMetaType::Int, 16 }, { QMetaType::QString, 17 },
        }}),
        // Slot 'adoptFreePanelApplet'
        QtMocHelpers::SlotData<QVariantMap(int, int)>(18, 2, QMC::AccessPublic, 0x80000000 | 14, {{
            { QMetaType::Int, 19 }, { QMetaType::Int, 20 },
        }}),
        // Slot 'saveFreePanelPosition'
        QtMocHelpers::SlotData<void(const QString &, int, int)>(21, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::QString, 9 }, { QMetaType::Int, 22 }, { QMetaType::Int, 23 },
        }}),
        // Slot 'setNativePanelType'
        QtMocHelpers::SlotData<bool(const QString &, const QString &)>(24, 2, QMC::AccessPublic, QMetaType::Bool, {{
            { QMetaType::QString, 9 }, { QMetaType::QString, 12 },
        }}),
        // Slot 'dockConfiguration'
        QtMocHelpers::SlotData<QVariantMap(const QString &) const>(25, 2, QMC::AccessPublic, 0x80000000 | 14, {{
            { QMetaType::QString, 9 },
        }}),
        // Slot 'setDockStringConfiguration'
        QtMocHelpers::SlotData<bool(const QString &, const QString &, const QString &)>(26, 2, QMC::AccessPublic, QMetaType::Bool, {{
            { QMetaType::QString, 9 }, { QMetaType::QString, 27 }, { QMetaType::QString, 28 },
        }}),
        // Slot 'setDockIntegerConfiguration'
        QtMocHelpers::SlotData<bool(const QString &, const QString &, int)>(29, 2, QMC::AccessPublic, QMetaType::Bool, {{
            { QMetaType::QString, 9 }, { QMetaType::QString, 27 }, { QMetaType::Int, 28 },
        }}),
        // Slot 'setDockRealConfiguration'
        QtMocHelpers::SlotData<bool(const QString &, const QString &, double)>(30, 2, QMC::AccessPublic, QMetaType::Bool, {{
            { QMetaType::QString, 9 }, { QMetaType::QString, 27 }, { QMetaType::Double, 28 },
        }}),
        // Slot 'setDockBooleanConfiguration'
        QtMocHelpers::SlotData<bool(const QString &, const QString &, bool)>(31, 2, QMC::AccessPublic, QMetaType::Bool, {{
            { QMetaType::QString, 9 }, { QMetaType::QString, 27 }, { QMetaType::Bool, 28 },
        }}),
        // Slot 'dockEntries'
        QtMocHelpers::SlotData<QVariantList(const QString &) const>(32, 2, QMC::AccessPublic, 0x80000000 | 33, {{
            { QMetaType::QString, 34 },
        }}),
        // Slot 'dockEntriesForPanel'
        QtMocHelpers::SlotData<QVariantList(const QString &, const QString &) const>(35, 2, QMC::AccessPublic, 0x80000000 | 33, {{
            { QMetaType::QString, 9 }, { QMetaType::QString, 34 },
        }}),
        // Slot 'activateDockEntry'
        QtMocHelpers::SlotData<bool(const QString &)>(36, 2, QMC::AccessPublic, QMetaType::Bool, {{
            { QMetaType::QString, 37 },
        }}),
        // Slot 'activateDockWindow'
        QtMocHelpers::SlotData<bool(const QString &, const QString &)>(38, 2, QMC::AccessPublic, QMetaType::Bool, {{
            { QMetaType::QString, 37 }, { QMetaType::QString, 39 },
        }}),
        // Slot 'minimizeDockEntry'
        QtMocHelpers::SlotData<bool(const QString &)>(40, 2, QMC::AccessPublic, QMetaType::Bool, {{
            { QMetaType::QString, 37 },
        }}),
        // Slot 'closeDockEntry'
        QtMocHelpers::SlotData<bool(const QString &)>(41, 2, QMC::AccessPublic, QMetaType::Bool, {{
            { QMetaType::QString, 37 },
        }}),
        // Slot 'closeAllDockEntry'
        QtMocHelpers::SlotData<bool(const QString &)>(42, 2, QMC::AccessPublic, QMetaType::Bool, {{
            { QMetaType::QString, 37 },
        }}),
        // Slot 'togglePinnedDockEntry'
        QtMocHelpers::SlotData<bool(const QString &)>(43, 2, QMC::AccessPublic, QMetaType::Bool, {{
            { QMetaType::QString, 37 },
        }}),
        // Slot 'moveDockEntryBefore'
        QtMocHelpers::SlotData<bool(const QString &, const QString &)>(44, 2, QMC::AccessPublic, QMetaType::Bool, {{
            { QMetaType::QString, 37 }, { QMetaType::QString, 45 },
        }}),
        // Slot 'pinDockUrl'
        QtMocHelpers::SlotData<bool(const QString &)>(46, 2, QMC::AccessPublic, QMetaType::Bool, {{
            { QMetaType::QString, 47 },
        }}),
        // Slot 'pinDockUrls'
        QtMocHelpers::SlotData<bool(const QStringList &)>(48, 2, QMC::AccessPublic, QMetaType::Bool, {{
            { QMetaType::QStringList, 49 },
        }}),
        // Slot 'pinPanelUrls'
        QtMocHelpers::SlotData<bool(const QString &, const QStringList &)>(50, 2, QMC::AccessPublic, QMetaType::Bool, {{
            { QMetaType::QString, 9 }, { QMetaType::QStringList, 49 },
        }}),
        // Slot 'removePanelContent'
        QtMocHelpers::SlotData<bool(const QString &, const QString &)>(51, 2, QMC::AccessPublic, QMetaType::Bool, {{
            { QMetaType::QString, 9 }, { QMetaType::QString, 52 },
        }}),
        // Slot 'dockFolderEntries'
        QtMocHelpers::SlotData<QVariantList(const QString &) const>(53, 2, QMC::AccessPublic, 0x80000000 | 33, {{
            { QMetaType::QString, 37 },
        }}),
        // Slot 'openDockUrl'
        QtMocHelpers::SlotData<bool(const QString &)>(54, 2, QMC::AccessPublic, QMetaType::Bool, {{
            { QMetaType::QString, 47 },
        }}),
        // Slot 'availableKdeWidgets'
        QtMocHelpers::SlotData<QStringList() const>(55, 2, QMC::AccessPublic, QMetaType::QStringList),
        // Slot 'createNativeKdePanel'
        QtMocHelpers::SlotData<bool(const QString &)>(56, 2, QMC::AccessPublic, QMetaType::Bool, {{
            { QMetaType::QString, 9 },
        }}),
        // Slot 'addKdeWidget'
        QtMocHelpers::SlotData<bool(const QString &, const QString &)>(57, 2, QMC::AccessPublic, QMetaType::Bool, {{
            { QMetaType::QString, 9 }, { QMetaType::QString, 58 },
        }}),
        // Slot 'removeNativeKdePanel'
        QtMocHelpers::SlotData<bool(const QString &)>(59, 2, QMC::AccessPublic, QMetaType::Bool, {{
            { QMetaType::QString, 9 },
        }}),
        // Slot 'removePanel'
        QtMocHelpers::SlotData<void(const QString &)>(60, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::QString, 9 },
        }}),
        // Slot 'openKdeWidgetPreview'
        QtMocHelpers::SlotData<bool(const QString &, const QString &)>(61, 2, QMC::AccessPublic, QMetaType::Bool, {{
            { QMetaType::QString, 9 }, { QMetaType::QString, 58 },
        }}),
        // Slot 'availableScreens'
        QtMocHelpers::SlotData<QVariantList() const>(62, 2, QMC::AccessPublic, 0x80000000 | 33),
        // Slot 'screenIndexForPanel'
        QtMocHelpers::SlotData<int(const QString &) const>(63, 2, QMC::AccessPublic, QMetaType::Int, {{
            { QMetaType::QString, 9 },
        }}),
        // Slot 'setPanelScreen'
        QtMocHelpers::SlotData<void(const QString &, int)>(64, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::QString, 9 }, { QMetaType::Int, 65 },
        }}),
        // Slot 'setPanelVisible'
        QtMocHelpers::SlotData<bool(const QString &, bool)>(66, 2, QMC::AccessPublic, QMetaType::Bool, {{
            { QMetaType::QString, 9 }, { QMetaType::Bool, 67 },
        }}),
        // Slot 'setPanelVisibilityMode'
        QtMocHelpers::SlotData<void(const QString &, const QString &)>(68, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::QString, 9 }, { QMetaType::QString, 69 },
        }}),
        // Slot 'shouldConcealPanel'
        QtMocHelpers::SlotData<bool(const QString &) const>(70, 2, QMC::AccessPublic, QMetaType::Bool, {{
            { QMetaType::QString, 9 },
        }}),
        // Slot 'resetSettings'
        QtMocHelpers::SlotData<void()>(71, 2, QMC::AccessPublic, QMetaType::Void),
        // Slot 'toggleAutoHide'
        QtMocHelpers::SlotData<void()>(72, 2, QMC::AccessPublic, QMetaType::Void),
        // Slot 'toggleDesktopSuite'
        QtMocHelpers::SlotData<void()>(73, 2, QMC::AccessPublic, QMetaType::Void),
        // Slot 'toggleTopLauncher'
        QtMocHelpers::SlotData<void()>(74, 2, QMC::AccessPublic, QMetaType::Void),
        // Slot 'toggleSideRail'
        QtMocHelpers::SlotData<void()>(75, 2, QMC::AccessPublic, QMetaType::Void),
        // Slot 'toggleBottomPanel'
        QtMocHelpers::SlotData<void()>(76, 2, QMC::AccessPublic, QMetaType::Void),
        // Slot 'applyProfile'
        QtMocHelpers::SlotData<void(const QString &)>(77, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::QString, 78 },
        }}),
        // Slot 'openSystemSettings'
        QtMocHelpers::SlotData<void(const QString &)>(79, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::QString, 80 },
        }}),
        // Slot 'showIconProperties'
        QtMocHelpers::SlotData<void(int)>(81, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::Int, 82 },
        }}),
    };
    QtMocHelpers::UintData qt_properties {
        // property 'screenRevision'
        QtMocHelpers::PropertyData<int>(83, QMetaType::Int, QMC::DefaultPropertyFlags, 0),
        // property 'visibilityRevision'
        QtMocHelpers::PropertyData<int>(84, QMetaType::Int, QMC::DefaultPropertyFlags, 1),
        // property 'dockRevision'
        QtMocHelpers::PropertyData<qulonglong>(85, QMetaType::ULongLong, QMC::DefaultPropertyFlags, 2),
        // property 'dockEntriesRevision'
        QtMocHelpers::PropertyData<qulonglong>(86, QMetaType::ULongLong, QMC::DefaultPropertyFlags, 3),
    };
    QtMocHelpers::UintData qt_enums {
    };
    return QtMocHelpers::metaObjectData<PanelWindow, qt_meta_tag_ZN11PanelWindowE_t>(QMC::MetaObjectFlag{}, qt_stringData,
            qt_methods, qt_properties, qt_enums);
}
Q_CONSTINIT const QMetaObject PanelWindow::staticMetaObject = { {
    QMetaObject::SuperData::link<QObject::staticMetaObject>(),
    qt_staticMetaObjectStaticContent<qt_meta_tag_ZN11PanelWindowE_t>.stringdata,
    qt_staticMetaObjectStaticContent<qt_meta_tag_ZN11PanelWindowE_t>.data,
    qt_static_metacall,
    nullptr,
    qt_staticMetaObjectRelocatingContent<qt_meta_tag_ZN11PanelWindowE_t>.metaTypes,
    nullptr
} };

void PanelWindow::qt_static_metacall(QObject *_o, QMetaObject::Call _c, int _id, void **_a)
{
    auto *_t = static_cast<PanelWindow *>(_o);
    if (_c == QMetaObject::InvokeMetaMethod) {
        switch (_id) {
        case 0: _t->screenRevisionChanged(); break;
        case 1: _t->visibilityRevisionChanged(); break;
        case 2: _t->dockRevisionChanged(); break;
        case 3: _t->dockEntriesRevisionChanged(); break;
        case 4: _t->nativePanelRecoveryFinished(); break;
        case 5: _t->showSettings(); break;
        case 6: _t->showPanelSettings((*reinterpret_cast<std::add_pointer_t<QString>>(_a[1]))); break;
        case 7: { QString _r = _t->createNativePanel((*reinterpret_cast<std::add_pointer_t<QString>>(_a[1])),(*reinterpret_cast<std::add_pointer_t<QString>>(_a[2])));
            if (_a[0]) *reinterpret_cast<QString*>(_a[0]) = std::move(_r); }  break;
        case 8: { QVariantMap _r = _t->createFreePanel();
            if (_a[0]) *reinterpret_cast<QVariantMap*>(_a[0]) = std::move(_r); }  break;
        case 9: { QVariantMap _r = _t->createFreePanelFromTemplate((*reinterpret_cast<std::add_pointer_t<int>>(_a[1])),(*reinterpret_cast<std::add_pointer_t<QString>>(_a[2])));
            if (_a[0]) *reinterpret_cast<QVariantMap*>(_a[0]) = std::move(_r); }  break;
        case 10: { QVariantMap _r = _t->adoptFreePanelApplet((*reinterpret_cast<std::add_pointer_t<int>>(_a[1])),(*reinterpret_cast<std::add_pointer_t<int>>(_a[2])));
            if (_a[0]) *reinterpret_cast<QVariantMap*>(_a[0]) = std::move(_r); }  break;
        case 11: _t->saveFreePanelPosition((*reinterpret_cast<std::add_pointer_t<QString>>(_a[1])),(*reinterpret_cast<std::add_pointer_t<int>>(_a[2])),(*reinterpret_cast<std::add_pointer_t<int>>(_a[3]))); break;
        case 12: { bool _r = _t->setNativePanelType((*reinterpret_cast<std::add_pointer_t<QString>>(_a[1])),(*reinterpret_cast<std::add_pointer_t<QString>>(_a[2])));
            if (_a[0]) *reinterpret_cast<bool*>(_a[0]) = std::move(_r); }  break;
        case 13: { QVariantMap _r = _t->dockConfiguration((*reinterpret_cast<std::add_pointer_t<QString>>(_a[1])));
            if (_a[0]) *reinterpret_cast<QVariantMap*>(_a[0]) = std::move(_r); }  break;
        case 14: { bool _r = _t->setDockStringConfiguration((*reinterpret_cast<std::add_pointer_t<QString>>(_a[1])),(*reinterpret_cast<std::add_pointer_t<QString>>(_a[2])),(*reinterpret_cast<std::add_pointer_t<QString>>(_a[3])));
            if (_a[0]) *reinterpret_cast<bool*>(_a[0]) = std::move(_r); }  break;
        case 15: { bool _r = _t->setDockIntegerConfiguration((*reinterpret_cast<std::add_pointer_t<QString>>(_a[1])),(*reinterpret_cast<std::add_pointer_t<QString>>(_a[2])),(*reinterpret_cast<std::add_pointer_t<int>>(_a[3])));
            if (_a[0]) *reinterpret_cast<bool*>(_a[0]) = std::move(_r); }  break;
        case 16: { bool _r = _t->setDockRealConfiguration((*reinterpret_cast<std::add_pointer_t<QString>>(_a[1])),(*reinterpret_cast<std::add_pointer_t<QString>>(_a[2])),(*reinterpret_cast<std::add_pointer_t<double>>(_a[3])));
            if (_a[0]) *reinterpret_cast<bool*>(_a[0]) = std::move(_r); }  break;
        case 17: { bool _r = _t->setDockBooleanConfiguration((*reinterpret_cast<std::add_pointer_t<QString>>(_a[1])),(*reinterpret_cast<std::add_pointer_t<QString>>(_a[2])),(*reinterpret_cast<std::add_pointer_t<bool>>(_a[3])));
            if (_a[0]) *reinterpret_cast<bool*>(_a[0]) = std::move(_r); }  break;
        case 18: { QVariantList _r = _t->dockEntries((*reinterpret_cast<std::add_pointer_t<QString>>(_a[1])));
            if (_a[0]) *reinterpret_cast<QVariantList*>(_a[0]) = std::move(_r); }  break;
        case 19: { QVariantList _r = _t->dockEntriesForPanel((*reinterpret_cast<std::add_pointer_t<QString>>(_a[1])),(*reinterpret_cast<std::add_pointer_t<QString>>(_a[2])));
            if (_a[0]) *reinterpret_cast<QVariantList*>(_a[0]) = std::move(_r); }  break;
        case 20: { bool _r = _t->activateDockEntry((*reinterpret_cast<std::add_pointer_t<QString>>(_a[1])));
            if (_a[0]) *reinterpret_cast<bool*>(_a[0]) = std::move(_r); }  break;
        case 21: { bool _r = _t->activateDockWindow((*reinterpret_cast<std::add_pointer_t<QString>>(_a[1])),(*reinterpret_cast<std::add_pointer_t<QString>>(_a[2])));
            if (_a[0]) *reinterpret_cast<bool*>(_a[0]) = std::move(_r); }  break;
        case 22: { bool _r = _t->minimizeDockEntry((*reinterpret_cast<std::add_pointer_t<QString>>(_a[1])));
            if (_a[0]) *reinterpret_cast<bool*>(_a[0]) = std::move(_r); }  break;
        case 23: { bool _r = _t->closeDockEntry((*reinterpret_cast<std::add_pointer_t<QString>>(_a[1])));
            if (_a[0]) *reinterpret_cast<bool*>(_a[0]) = std::move(_r); }  break;
        case 24: { bool _r = _t->closeAllDockEntry((*reinterpret_cast<std::add_pointer_t<QString>>(_a[1])));
            if (_a[0]) *reinterpret_cast<bool*>(_a[0]) = std::move(_r); }  break;
        case 25: { bool _r = _t->togglePinnedDockEntry((*reinterpret_cast<std::add_pointer_t<QString>>(_a[1])));
            if (_a[0]) *reinterpret_cast<bool*>(_a[0]) = std::move(_r); }  break;
        case 26: { bool _r = _t->moveDockEntryBefore((*reinterpret_cast<std::add_pointer_t<QString>>(_a[1])),(*reinterpret_cast<std::add_pointer_t<QString>>(_a[2])));
            if (_a[0]) *reinterpret_cast<bool*>(_a[0]) = std::move(_r); }  break;
        case 27: { bool _r = _t->pinDockUrl((*reinterpret_cast<std::add_pointer_t<QString>>(_a[1])));
            if (_a[0]) *reinterpret_cast<bool*>(_a[0]) = std::move(_r); }  break;
        case 28: { bool _r = _t->pinDockUrls((*reinterpret_cast<std::add_pointer_t<QStringList>>(_a[1])));
            if (_a[0]) *reinterpret_cast<bool*>(_a[0]) = std::move(_r); }  break;
        case 29: { bool _r = _t->pinPanelUrls((*reinterpret_cast<std::add_pointer_t<QString>>(_a[1])),(*reinterpret_cast<std::add_pointer_t<QStringList>>(_a[2])));
            if (_a[0]) *reinterpret_cast<bool*>(_a[0]) = std::move(_r); }  break;
        case 30: { bool _r = _t->removePanelContent((*reinterpret_cast<std::add_pointer_t<QString>>(_a[1])),(*reinterpret_cast<std::add_pointer_t<QString>>(_a[2])));
            if (_a[0]) *reinterpret_cast<bool*>(_a[0]) = std::move(_r); }  break;
        case 31: { QVariantList _r = _t->dockFolderEntries((*reinterpret_cast<std::add_pointer_t<QString>>(_a[1])));
            if (_a[0]) *reinterpret_cast<QVariantList*>(_a[0]) = std::move(_r); }  break;
        case 32: { bool _r = _t->openDockUrl((*reinterpret_cast<std::add_pointer_t<QString>>(_a[1])));
            if (_a[0]) *reinterpret_cast<bool*>(_a[0]) = std::move(_r); }  break;
        case 33: { QStringList _r = _t->availableKdeWidgets();
            if (_a[0]) *reinterpret_cast<QStringList*>(_a[0]) = std::move(_r); }  break;
        case 34: { bool _r = _t->createNativeKdePanel((*reinterpret_cast<std::add_pointer_t<QString>>(_a[1])));
            if (_a[0]) *reinterpret_cast<bool*>(_a[0]) = std::move(_r); }  break;
        case 35: { bool _r = _t->addKdeWidget((*reinterpret_cast<std::add_pointer_t<QString>>(_a[1])),(*reinterpret_cast<std::add_pointer_t<QString>>(_a[2])));
            if (_a[0]) *reinterpret_cast<bool*>(_a[0]) = std::move(_r); }  break;
        case 36: { bool _r = _t->removeNativeKdePanel((*reinterpret_cast<std::add_pointer_t<QString>>(_a[1])));
            if (_a[0]) *reinterpret_cast<bool*>(_a[0]) = std::move(_r); }  break;
        case 37: _t->removePanel((*reinterpret_cast<std::add_pointer_t<QString>>(_a[1]))); break;
        case 38: { bool _r = _t->openKdeWidgetPreview((*reinterpret_cast<std::add_pointer_t<QString>>(_a[1])),(*reinterpret_cast<std::add_pointer_t<QString>>(_a[2])));
            if (_a[0]) *reinterpret_cast<bool*>(_a[0]) = std::move(_r); }  break;
        case 39: { QVariantList _r = _t->availableScreens();
            if (_a[0]) *reinterpret_cast<QVariantList*>(_a[0]) = std::move(_r); }  break;
        case 40: { int _r = _t->screenIndexForPanel((*reinterpret_cast<std::add_pointer_t<QString>>(_a[1])));
            if (_a[0]) *reinterpret_cast<int*>(_a[0]) = std::move(_r); }  break;
        case 41: _t->setPanelScreen((*reinterpret_cast<std::add_pointer_t<QString>>(_a[1])),(*reinterpret_cast<std::add_pointer_t<int>>(_a[2]))); break;
        case 42: { bool _r = _t->setPanelVisible((*reinterpret_cast<std::add_pointer_t<QString>>(_a[1])),(*reinterpret_cast<std::add_pointer_t<bool>>(_a[2])));
            if (_a[0]) *reinterpret_cast<bool*>(_a[0]) = std::move(_r); }  break;
        case 43: _t->setPanelVisibilityMode((*reinterpret_cast<std::add_pointer_t<QString>>(_a[1])),(*reinterpret_cast<std::add_pointer_t<QString>>(_a[2]))); break;
        case 44: { bool _r = _t->shouldConcealPanel((*reinterpret_cast<std::add_pointer_t<QString>>(_a[1])));
            if (_a[0]) *reinterpret_cast<bool*>(_a[0]) = std::move(_r); }  break;
        case 45: _t->resetSettings(); break;
        case 46: _t->toggleAutoHide(); break;
        case 47: _t->toggleDesktopSuite(); break;
        case 48: _t->toggleTopLauncher(); break;
        case 49: _t->toggleSideRail(); break;
        case 50: _t->toggleBottomPanel(); break;
        case 51: _t->applyProfile((*reinterpret_cast<std::add_pointer_t<QString>>(_a[1]))); break;
        case 52: _t->openSystemSettings((*reinterpret_cast<std::add_pointer_t<QString>>(_a[1]))); break;
        case 53: _t->showIconProperties((*reinterpret_cast<std::add_pointer_t<int>>(_a[1]))); break;
        default: ;
        }
    }
    if (_c == QMetaObject::IndexOfMethod) {
        if (QtMocHelpers::indexOfMethod<void (PanelWindow::*)()>(_a, &PanelWindow::screenRevisionChanged, 0))
            return;
        if (QtMocHelpers::indexOfMethod<void (PanelWindow::*)()>(_a, &PanelWindow::visibilityRevisionChanged, 1))
            return;
        if (QtMocHelpers::indexOfMethod<void (PanelWindow::*)()>(_a, &PanelWindow::dockRevisionChanged, 2))
            return;
        if (QtMocHelpers::indexOfMethod<void (PanelWindow::*)()>(_a, &PanelWindow::dockEntriesRevisionChanged, 3))
            return;
        if (QtMocHelpers::indexOfMethod<void (PanelWindow::*)()>(_a, &PanelWindow::nativePanelRecoveryFinished, 4))
            return;
    }
    if (_c == QMetaObject::ReadProperty) {
        void *_v = _a[0];
        switch (_id) {
        case 0: *reinterpret_cast<int*>(_v) = _t->screenRevision(); break;
        case 1: *reinterpret_cast<int*>(_v) = _t->visibilityRevision(); break;
        case 2: *reinterpret_cast<qulonglong*>(_v) = _t->dockRevision(); break;
        case 3: *reinterpret_cast<qulonglong*>(_v) = _t->dockEntriesRevision(); break;
        default: break;
        }
    }
}

const QMetaObject *PanelWindow::metaObject() const
{
    return QObject::d_ptr->metaObject ? QObject::d_ptr->dynamicMetaObject() : &staticMetaObject;
}

void *PanelWindow::qt_metacast(const char *_clname)
{
    if (!_clname) return nullptr;
    if (!strcmp(_clname, qt_staticMetaObjectStaticContent<qt_meta_tag_ZN11PanelWindowE_t>.strings))
        return static_cast<void*>(this);
    return QObject::qt_metacast(_clname);
}

int PanelWindow::qt_metacall(QMetaObject::Call _c, int _id, void **_a)
{
    _id = QObject::qt_metacall(_c, _id, _a);
    if (_id < 0)
        return _id;
    if (_c == QMetaObject::InvokeMetaMethod) {
        if (_id < 54)
            qt_static_metacall(this, _c, _id, _a);
        _id -= 54;
    }
    if (_c == QMetaObject::RegisterMethodArgumentMetaType) {
        if (_id < 54)
            *reinterpret_cast<QMetaType *>(_a[0]) = QMetaType();
        _id -= 54;
    }
    if (_c == QMetaObject::ReadProperty || _c == QMetaObject::WriteProperty
            || _c == QMetaObject::ResetProperty || _c == QMetaObject::BindableProperty
            || _c == QMetaObject::RegisterPropertyMetaType) {
        qt_static_metacall(this, _c, _id, _a);
        _id -= 4;
    }
    return _id;
}

// SIGNAL 0
void PanelWindow::screenRevisionChanged()
{
    QMetaObject::activate(this, &staticMetaObject, 0, nullptr);
}

// SIGNAL 1
void PanelWindow::visibilityRevisionChanged()
{
    QMetaObject::activate(this, &staticMetaObject, 1, nullptr);
}

// SIGNAL 2
void PanelWindow::dockRevisionChanged()
{
    QMetaObject::activate(this, &staticMetaObject, 2, nullptr);
}

// SIGNAL 3
void PanelWindow::dockEntriesRevisionChanged()
{
    QMetaObject::activate(this, &staticMetaObject, 3, nullptr);
}

// SIGNAL 4
void PanelWindow::nativePanelRecoveryFinished()
{
    QMetaObject::activate(this, &staticMetaObject, 4, nullptr);
}
QT_WARNING_POP
