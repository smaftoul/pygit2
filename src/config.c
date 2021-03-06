/*
 * Copyright 2010-2012 The pygit2 contributors
 *
 * This file is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License, version 2,
 * as published by the Free Software Foundation.
 *
 * In addition to the permissions in the GNU General Public License,
 * the authors give you unlimited permission to link the compiled
 * version of this file into combinations with other programs,
 * and to distribute those combinations without any restriction
 * coming from the use of this file.  (The General Public License
 * restrictions do apply in other respects; for example, they cover
 * modification of the file, and distribution when not linked into
 * a combined executable.)
 *
 * This file is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; see the file COPYING.  If not, write to
 * the Free Software Foundation, 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#define PY_SSIZE_T_CLEAN
#include <Python.h>
#include <pygit2/error.h>
#include <pygit2/types.h>
#include <pygit2/utils.h>
#include <pygit2/config.h>

extern PyTypeObject ConfigType;

int
Config_init(Config *self, PyObject *args, PyObject *kwds)
{
    char *path;
    int err;

    if (kwds) {
        PyErr_SetString(PyExc_TypeError,
                        "Repository takes no keyword arguments");
        return -1;
    }

    if (PySequence_Length(args) > 0) {
        if (!PyArg_ParseTuple(args, "s", &path)) {
            return -1;
        }

        err = git_config_open_ondisk(&self->config, path);

    } else {
        err = git_config_new(&self->config);
    }

    if (err < 0) {
        if (err == GIT_ENOTFOUND) {
            Error_set_exc(PyExc_IOError);
        } else {
            Error_set(err);
        }

        return -1;
    }

    return 0;
}

void
Config_dealloc(Config *self)
{
    PyObject_GC_UnTrack(self);
    Py_XDECREF(self->repo);
    git_config_free(self->config);
    PyObject_GC_Del(self);
}

int
Config_traverse(Config *self, visitproc visit, void *arg)
{
    Py_VISIT(self->repo);
    return 0;
}

PyObject *
Config_open(char *c_path) {
    PyObject *py_path = Py_BuildValue("(s)", c_path);
    Config *config = PyObject_GC_New(Config, &ConfigType);

    Config_init(config, py_path, NULL);

    Py_INCREF(config);

    return (PyObject *)config;
}


PyDoc_STRVAR(Config_get_global_config__doc__,
  "get_global_config() -> Config\n"
  "\n"
  "Return an object representing the global configuration file.");

PyObject *
Config_get_global_config(void)
{
    char path[GIT_PATH_MAX];
    int err;

    err = git_config_find_global(path, GIT_PATH_MAX);
    if (err < 0) {
        if (err == GIT_ENOTFOUND) {
            PyErr_SetString(PyExc_IOError, "Global config file not found.");
            return NULL;
        }
        return Error_set(err);
    }

    return Config_open(path);
}


PyDoc_STRVAR(Config_get_system_config__doc__,
  "get_system_config() -> Config\n"
  "\n"
  "Return an object representing the system configuration file.");

PyObject *
Config_get_system_config(void)
{
    char path[GIT_PATH_MAX];
    int err;

    err = git_config_find_system(path, GIT_PATH_MAX);
    if (err < 0) {
        if (err == GIT_ENOTFOUND) {
            PyErr_SetString(PyExc_IOError, "System config file not found.");
            return NULL;
        }
        return Error_set(err);
    }

    return Config_open(path);
}

int
Config_contains(Config *self, PyObject *py_key) {
    int err;
    const char *c_value;
    char *c_key;

    c_key = py_str_to_c_str(py_key,NULL);
    if (c_key == NULL)
        return -1;

    err = git_config_get_string(&c_value, self->config, c_key);
    free(c_key);
    if (err == GIT_ENOTFOUND)
        return 0;
    if (err < 0) {
        Error_set(err);
        return -1;
    }

    return 1;
}

PyObject *
Config_getitem(Config *self, PyObject *py_key)
{
    int err;
    int64_t c_intvalue;
    int c_boolvalue;
    const char *c_charvalue;
    char *c_key;

    if (!(c_key = py_str_to_c_str(py_key,NULL)))
        return NULL;

    err = git_config_get_int64(&c_intvalue, self->config, c_key);
    if (err == GIT_OK) {
        free(c_key);
        return PyInt_FromLong((long)c_intvalue);
    }

    err = git_config_get_bool(&c_boolvalue, self->config, c_key);
    if (err == GIT_OK) {
        free(c_key);
        return PyBool_FromLong((long)c_boolvalue);
    }

    err = git_config_get_string(&c_charvalue, self->config, c_key);
    free(c_key);
    if (err < 0) {
        if (err == GIT_ENOTFOUND) {
            PyErr_SetObject(PyExc_KeyError, py_key);
            return NULL;
        }
        return Error_set(err);
    }

    return PyUnicode_FromString(c_charvalue);
}

int
Config_setitem(Config *self, PyObject *py_key, PyObject *py_value)
{
    int err;
    char *c_key;
    char *py_str;

    if (!(c_key = py_str_to_c_str(py_key,NULL)))
        return -1;

    if (!py_value) {
        err = git_config_delete_entry(self->config, c_key);
    } else if (PyBool_Check(py_value)) {
        err = git_config_set_bool(self->config, c_key,
                (int)PyObject_IsTrue(py_value));
    } else if (PyInt_Check(py_value)) {
        err = git_config_set_int64(self->config, c_key,
                (int64_t)PyInt_AsLong(py_value));
    } else {
        py_value = PyObject_Str(py_value);
        py_str = py_str_to_c_str(py_value,NULL);
        err = git_config_set_string(self->config, c_key, py_str);
        free(py_str);
    }

    free(c_key);
    if (err < 0) {
        Error_set(err);
        return -1;
    }
    return 0;
}

int
Config_foreach_callback_wrapper(const git_config_entry *entry, void *c_payload)
{
    PyObject *args = (PyObject *)c_payload;
    PyObject *py_callback = NULL;
    PyObject *py_payload = NULL;
    PyObject *py_result = NULL;
    int c_result;

    if (!PyArg_ParseTuple(args, "O|O", &py_callback, &py_payload))
        return -1;

    if (py_payload)
        args = Py_BuildValue("ssO", entry->name, entry->value, py_payload);
    else
        args = Py_BuildValue("ss", entry->name, entry->value);
    if (!args)
        return -1;

    if (!(py_result = PyObject_CallObject(py_callback,args)))
        return -1;

    if ((c_result = PyLong_AsLong(py_result) == -1))
        return -1;

    return c_result;
}


PyDoc_STRVAR(Config_foreach__doc__,
  "foreach(callback[, payload]) -> int\n"
  "\n"
  "Perform an operation on each config variable.\n"
  "\n"
  "The callback must be of type Callable and receives the normalized name\n"
  "and value of each variable in the config backend, and an optional payload\n"
  "passed to this method. As soon as one of the callbacks returns an integer\n"
  "other than 0, this function returns that value.");

PyObject *
Config_foreach(Config *self, PyObject *args)
{
    int ret;
    PyObject *py_callback;
    PyObject *py_payload;

    if (!PyArg_ParseTuple(args, "O|O", &py_callback, &py_payload))
        return NULL;

    if (!PyCallable_Check(py_callback)) {
        PyErr_SetString(PyExc_TypeError,
                        "Argument 'callback' is not callable");
        return NULL;
    }

    ret = git_config_foreach(self->config, Config_foreach_callback_wrapper,
            (void *)args);

    return PyInt_FromLong((long)ret);
}


PyDoc_STRVAR(Config_add_file__doc__,
  "add_file(path, level=0, force=0)\n"
  "\n"
  "Add a config file instance to an existing config.");

PyObject *
Config_add_file(Config *self, PyObject *args, PyObject *kwds)
{
    char *keywords[] = {"path", "level", "force", NULL};
    int err;
    char *path;
    unsigned int level = 0;
    int force = 0;

    if (!PyArg_ParseTupleAndKeywords(args, kwds, "s|Ii", keywords,
                                     &path, &level, &force))
        return NULL;

    err = git_config_add_file_ondisk(self->config, path, level, force);
    if (err < 0) {
        Error_set_str(err, path);
        return NULL;
    }

    Py_RETURN_NONE;
}


PyDoc_STRVAR(Config_get_multivar__doc__,
  "get_multivar(name[, regex]) -> [str, ...]\n"
  "\n"
  "Get each value of a multivar ''name'' as a list. The optional ''regex''\n"
  "parameter is expected to be a regular expression to filter the variables\n"
  "we're interested in.");

int
Config_get_multivar_fn_wrapper(const git_config_entry *value, void *data)
{
    PyObject *list = (PyObject *)data;
    PyObject *item = NULL;

    if (!(item = PyUnicode_FromString(value->value)))
        return -2;

    PyList_Append(list, item);

    return 0;
}

PyObject *
Config_get_multivar(Config *self, PyObject *args)
{
    int err;
    PyObject *list = PyList_New(0);
    const char *name = NULL;
    const char *regex = NULL;

    if (!PyArg_ParseTuple(args, "s|s", &name, &regex))
        return NULL;

    err = git_config_get_multivar(self->config, name, regex,
                                  Config_get_multivar_fn_wrapper,
                                  (void *)list);
    if (err  < 0) {
        if (err == GIT_ENOTFOUND)
            Error_set(err);
        else
            PyErr_SetNone(PyExc_TypeError);
        return NULL;
    }

    return list;
}


PyDoc_STRVAR(Config_set_multivar__doc__,
  "set_multivar(name, regex, value)\n"
  "\n"
  "Set a multivar ''name'' to ''value''. ''regexp'' is a regular expression\n"
  "to indicate which values to replace");

PyObject *
Config_set_multivar(Config *self, PyObject *args)
{
    int err;
    const char *name = NULL;
    const char *regex = NULL;
    const char *value = NULL;

    if (!PyArg_ParseTuple(args, "sss", &name, &regex, &value))
        return NULL;

    err = git_config_set_multivar(self->config, name, regex, value);
    if (err < 0) {
        if (err == GIT_ENOTFOUND)
            Error_set(err);
        else
            PyErr_SetNone(PyExc_TypeError);
        return NULL;
    }

    Py_RETURN_NONE;
}

PyMethodDef Config_methods[] = {
    METHOD(Config, get_system_config, METH_NOARGS | METH_STATIC),
    METHOD(Config, get_global_config, METH_NOARGS | METH_STATIC),
    METHOD(Config, foreach, METH_VARARGS),
    METHOD(Config, add_file, METH_VARARGS | METH_KEYWORDS),
    METHOD(Config, get_multivar, METH_VARARGS),
    METHOD(Config, set_multivar, METH_VARARGS),
    {NULL}
};

PySequenceMethods Config_as_sequence = {
    0,                          /* sq_length */
    0,                          /* sq_concat */
    0,                          /* sq_repeat */
    0,                          /* sq_item */
    0,                          /* sq_slice */
    0,                          /* sq_ass_item */
    0,                          /* sq_ass_slice */
    (objobjproc)Config_contains,/* sq_contains */
};

PyMappingMethods Config_as_mapping = {
    0,                               /* mp_length */
    (binaryfunc)Config_getitem,      /* mp_subscript */
    (objobjargproc)Config_setitem,   /* mp_ass_subscript */
};


PyDoc_STRVAR(Config__doc__, "Configuration management.");

PyTypeObject ConfigType = {
    PyVarObject_HEAD_INIT(NULL, 0)
    "_pygit2.Config",                          /* tp_name           */
    sizeof(Config),                            /* tp_basicsize      */
    0,                                         /* tp_itemsize       */
    (destructor)Config_dealloc,                /* tp_dealloc        */
    0,                                         /* tp_print          */
    0,                                         /* tp_getattr        */
    0,                                         /* tp_setattr        */
    0,                                         /* tp_compare        */
    0,                                         /* tp_repr           */
    0,                                         /* tp_as_number      */
    &Config_as_sequence,                       /* tp_as_sequence    */
    &Config_as_mapping,                        /* tp_as_mapping     */
    0,                                         /* tp_hash           */
    0,                                         /* tp_call           */
    0,                                         /* tp_str            */
    0,                                         /* tp_getattro       */
    0,                                         /* tp_setattro       */
    0,                                         /* tp_as_buffer      */
    Py_TPFLAGS_DEFAULT | Py_TPFLAGS_HAVE_GC,   /* tp_flags          */
    Config__doc__,                             /* tp_doc            */
    (traverseproc)Config_traverse,             /* tp_traverse       */
    0,                                         /* tp_clear          */
    0,                                         /* tp_richcompare    */
    0,                                         /* tp_weaklistoffset */
    0,                                         /* tp_iter           */
    0,                                         /* tp_iternext       */
    Config_methods,                            /* tp_methods        */
    0,                                         /* tp_members        */
    0,                                         /* tp_getset         */
    0,                                         /* tp_base           */
    0,                                         /* tp_dict           */
    0,                                         /* tp_descr_get      */
    0,                                         /* tp_descr_set      */
    0,                                         /* tp_dictoffset     */
    (initproc)Config_init,                     /* tp_init           */
    0,                                         /* tp_alloc          */
    0,                                         /* tp_new            */
};
