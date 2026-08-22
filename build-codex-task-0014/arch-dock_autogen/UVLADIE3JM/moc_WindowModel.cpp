/****************************************************************************
** Meta object code from reading C++ file 'WindowModel.h'
**
** Created by: The Qt Meta Object Compiler version 69 (Qt 6.11.2)
**
** WARNING! All changes made in this file will be lost!
*****************************************************************************/

#include "../../../src/WindowModel.h"
#include <QtCore/qmetatype.h>

#include <QtCore/qtmochelpers.h>

#include <memory>


#include <QtCore/qxptype_traits.h>
#if !defined(Q_MOC_OUTPUT_REVISION)
#error "The header file 'WindowModel.h' doesn't include <QObject>."
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
struct qt_meta_tag_ZN11WindowModelE_t {};
} // unnamed namespace

template <> constexpr inline auto WindowModel::qt_create_metaobjectdata<qt_meta_tag_ZN11WindowModelE_t>()
{
    namespace QMC = QtMocConstants;
    QtMocHelpers::StringRefStorage qt_stringData {
        "WindowModel",
        "Role",
        "InternalIdRole",
        "DesktopFileNameRole",
        "IconNameRole",
        "ResourceClassRole",
        "ResourceNameRole",
        "CaptionRole",
        "FrameGeometryRole",
        "ScreenIndexRole",
        "ActiveRole",
        "MinimizedRole",
        "MaximizedRole",
        "FullScreenRole"
    };

    QtMocHelpers::UintData qt_methods {
    };
    QtMocHelpers::UintData qt_properties {
    };
    QtMocHelpers::UintData qt_enums {
        // enum 'Role'
        QtMocHelpers::EnumData<enum Role>(1, 1, QMC::EnumFlags{}).add({
            {    2, Role::InternalIdRole },
            {    3, Role::DesktopFileNameRole },
            {    4, Role::IconNameRole },
            {    5, Role::ResourceClassRole },
            {    6, Role::ResourceNameRole },
            {    7, Role::CaptionRole },
            {    8, Role::FrameGeometryRole },
            {    9, Role::ScreenIndexRole },
            {   10, Role::ActiveRole },
            {   11, Role::MinimizedRole },
            {   12, Role::MaximizedRole },
            {   13, Role::FullScreenRole },
        }),
    };
    return QtMocHelpers::metaObjectData<WindowModel, qt_meta_tag_ZN11WindowModelE_t>(QMC::MetaObjectFlag{}, qt_stringData,
            qt_methods, qt_properties, qt_enums);
}
Q_CONSTINIT const QMetaObject WindowModel::staticMetaObject = { {
    QMetaObject::SuperData::link<QAbstractListModel::staticMetaObject>(),
    qt_staticMetaObjectStaticContent<qt_meta_tag_ZN11WindowModelE_t>.stringdata,
    qt_staticMetaObjectStaticContent<qt_meta_tag_ZN11WindowModelE_t>.data,
    qt_static_metacall,
    nullptr,
    qt_staticMetaObjectRelocatingContent<qt_meta_tag_ZN11WindowModelE_t>.metaTypes,
    nullptr
} };

void WindowModel::qt_static_metacall(QObject *_o, QMetaObject::Call _c, int _id, void **_a)
{
    auto *_t = static_cast<WindowModel *>(_o);
    (void)_t;
    (void)_c;
    (void)_id;
    (void)_a;
}

const QMetaObject *WindowModel::metaObject() const
{
    return QObject::d_ptr->metaObject ? QObject::d_ptr->dynamicMetaObject() : &staticMetaObject;
}

void *WindowModel::qt_metacast(const char *_clname)
{
    if (!_clname) return nullptr;
    if (!strcmp(_clname, qt_staticMetaObjectStaticContent<qt_meta_tag_ZN11WindowModelE_t>.strings))
        return static_cast<void*>(this);
    return QAbstractListModel::qt_metacast(_clname);
}

int WindowModel::qt_metacall(QMetaObject::Call _c, int _id, void **_a)
{
    _id = QAbstractListModel::qt_metacall(_c, _id, _a);
    return _id;
}
QT_WARNING_POP
