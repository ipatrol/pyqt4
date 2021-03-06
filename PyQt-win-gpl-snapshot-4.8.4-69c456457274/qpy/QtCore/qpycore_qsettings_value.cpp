// This implements the helper for QSettings.value().
//
// Copyright (c) 2011 Riverbank Computing Limited <info@riverbankcomputing.com>
// 
// This file is part of PyQt.
// 
// This file may be used under the terms of the GNU General Public
// License versions 2.0 or 3.0 as published by the Free Software
// Foundation and appearing in the files LICENSE.GPL2 and LICENSE.GPL3
// included in the packaging of this file.  Alternatively you may (at
// your option) use any later version of the GNU General Public
// License if such license has been publicly approved by Riverbank
// Computing Limited (or its successors, if any) and the KDE Free Qt
// Foundation. In addition, as a special exception, Riverbank gives you
// certain additional rights. These rights are described in the Riverbank
// GPL Exception version 1.1, which can be found in the file
// GPL_EXCEPTION.txt in this package.
// 
// Please review the following information to ensure GNU General
// Public Licensing requirements will be met:
// http://trolltech.com/products/qt/licenses/licensing/opensource/. If
// you are unsure which license is appropriate for your use, please
// review the following information:
// http://trolltech.com/products/qt/licenses/licensing/licensingoverview
// or contact the sales department at sales@riverbankcomputing.com.
// 
// This file is provided AS IS with NO WARRANTY OF ANY KIND, INCLUDING THE
// WARRANTY OF DESIGN, MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE.


#include <Python.h>

#include <QSettings>

#include "qpycore_chimera.h"
#include "qpycore_sip.h"


// Forward declarations.
#if QT_VERSION >= 0x040500
static PyObject *convert_hash(const Chimera *ct, const QVariantHash &value);
#endif
static PyObject *convert_list(const Chimera *ct, const QVariantList &value);
static PyObject *convert_map(const Chimera *ct, const QVariantMap &value);
static PyObject *convert(const Chimera *ct, const QVariant &value);
static int add_variant_to_dict(const Chimera *ct, PyObject *dict,
        const QString &key, const QVariant &value);


// Implement QSettings.value() and allow the type of the returned object to be
// explicitly specified.
PyObject *qpycore_qsettings_value(const QSettings *qset, const QString &key,
        const QVariant &defaultValue, PyObject *type)
{
    // Parse the type if one was given.
    const Chimera *ct;

    if (type)
    {
        ct = Chimera::parse(type);

        if (!ct)
            return 0;
    }

    // Get the value.
    QVariant value;
    PyObject *value_obj;

    // QSettings has an internal mutex so release the GIL to avoid the
    // possibility of deadlocks.
    Py_BEGIN_ALLOW_THREADS
    value = qset->value(key, defaultValue);
    Py_END_ALLOW_THREADS

    if (type)
    {
        // Get QVariant to do a conversion if there is one to do.
        if (ct->metatype() < static_cast<int>(QVariant::UserType))
        {
            QVariant::Type wanted = static_cast<QVariant::Type>(ct->metatype());

            // If we have a QStringList but are not wanting one then convert it
            // to a QVariantList.
            if (wanted != QVariant::StringList && value.type() == QVariant::StringList)
                value.convert(QVariant::List);

            // If we have a container but are not wanting one then assume we
            // want a container with elements of the wanted type.
            if (wanted != QVariant::List && value.type() == QVariant::List)
                value_obj = convert_list(ct, value.toList());
            else if (wanted != QVariant::Map && value.type() == QVariant::Map)
                value_obj = convert_map(ct, value.toMap());
#if QT_VERSION >= 0x040500
            else if (wanted != QVariant::Hash && value.type() == QVariant::Hash)
                value_obj = convert_hash(ct, value.toHash());
#endif
            else
                value_obj = convert(ct, value);
        }
        else
        {
            // This is likely to fail and the exception will say why.
            value_obj = ct->toPyObject(value);
        }

        delete ct;
    }
    else
    {
        QVariant *heap = new QVariant(value);
        value_obj = sipConvertFromNewType(heap, sipType_QVariant, 0);

        if (!value_obj)
            delete heap;
    }

    return value_obj;
}


// Convert a QVariantList to a list of Python objects.
static PyObject *convert_list(const Chimera *ct, const QVariantList &value)
{
    PyObject *list = PyList_New(value.size());

    if (!list)
        return 0;

    for (int i = 0; i < value.size(); ++i)
    {
        PyObject *el = convert(ct, value.at(i));

        if (!el)
        {
            Py_DECREF(list);
            return 0;
        }

        PyList_SET_ITEM(list, i, el);
    }

    return list;
}


// Convert a QVariantMap to a dict of Python objects.
static PyObject *convert_map(const Chimera *ct, const QVariantMap &value)
{
    PyObject *dict = PyDict_New();

    if (!dict)
        return 0;

    for (QVariantMap::const_iterator it = value.constBegin(); it != value.constEnd(); ++it)
    {
        if (add_variant_to_dict(ct, dict, it.key(), it.value()) < 0)
        {
            Py_DECREF(dict);
            return 0;
        }
    }

    return dict;
}


#if QT_VERSION >= 0x040500
// Convert a QVariantHash to a dict of Python objects.
static PyObject *convert_hash(const Chimera *ct, const QVariantHash &value)
{
    PyObject *dict = PyDict_New();

    if (!dict)
        return 0;

    for (QVariantHash::const_iterator it = value.constBegin(); it != value.constEnd(); ++it)
    {
        if (add_variant_to_dict(ct, dict, it.key(), it.value()) < 0)
        {
            Py_DECREF(dict);
            return 0;
        }
    }

    return dict;
}
#endif


// Convert a QVariant to a Python object.
static PyObject *convert(const Chimera *ct, const QVariant &value)
{
    QVariant converted = value;

    if (!converted.convert(static_cast<QVariant::Type>(ct->metatype())))
        converted = value;

    return ct->toPyObject(converted);
}


// Add a QVariant to a Python dict with a QString key.
static int add_variant_to_dict(const Chimera *ct, PyObject *dict,
        const QString &key, const QVariant &value)
{
    QString *key_heap = new QString(key);
    PyObject *key_obj = sipConvertFromNewType(key_heap, sipType_QString, 0);

    if (!key_obj)
    {
        delete key_heap;
        return 0;
    }

    PyObject *value_obj = convert(ct, value);

    if (!value_obj)
    {
        Py_DECREF(key_obj);
        return 0;
    }

    int rc = PyDict_SetItem(dict, key_obj, value_obj);

    Py_DECREF(key_obj);
    Py_DECREF(value_obj);

    return rc;
}
