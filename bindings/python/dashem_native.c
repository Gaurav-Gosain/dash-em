/**
 * @file dashem_native.c
 * @brief Python C extension for dash-em
 *
 * This module provides a low-level C extension interface to the
 * em-dash removal library for use by the dashem.py wrapper.
 */

#define PY_SSIZE_T_CLEAN
#include <Python.h>
#include "dashem.h"

/**
 * Python binding for dashem_remove
 */
static PyObject* dashem_native_remove(PyObject* self, PyObject* args) {
    const char* input;
    Py_ssize_t input_len;

    if (!PyArg_ParseTuple(args, "s#", &input, &input_len)) {
        return NULL;
    }

    /* Allocate output buffer */
    char* output = (char*)malloc(input_len + 1);
    if (!output) {
        return PyErr_NoMemory();
    }

    size_t output_len = 0;
    int result = dashem_remove(input, input_len, output, input_len, &output_len);

    if (result != 0) {
        free(output);
        PyErr_SetString(PyExc_RuntimeError, "dashem_remove failed");
        return NULL;
    }

    PyObject* py_result = PyUnicode_FromStringAndSize(output, output_len);
    free(output);

    return py_result;
}

/**
 * Python binding for dashem_version
 */
static PyObject* dashem_native_version(PyObject* self, PyObject* args) {
    const char* version = dashem_version();
    return PyUnicode_FromString(version);
}

/**
 * Python binding for dashem_implementation_name
 */
static PyObject* dashem_native_implementation_name(PyObject* self, PyObject* args) {
    const char* impl = dashem_implementation_name();
    return PyUnicode_FromString(impl);
}

/**
 * Python binding for dashem_detect_cpu_features
 */
static PyObject* dashem_native_detect_cpu_features(PyObject* self, PyObject* args) {
    uint32_t features = dashem_detect_cpu_features();
    return PyLong_FromUnsignedLong(features);
}

/**
 * Method table
 */
static PyMethodDef DashemMethods[] = {
    {"remove", dashem_native_remove, METH_VARARGS, "Remove em-dashes from a string"},
    {"version", dashem_native_version, METH_NOARGS, "Get library version"},
    {"implementation_name", dashem_native_implementation_name, METH_NOARGS, "Get implementation name"},
    {"detect_cpu_features", dashem_native_detect_cpu_features, METH_NOARGS, "Detect CPU features"},
    {NULL, NULL, 0, NULL}
};

/**
 * Module definition
 */
static struct PyModuleDef dashem_native_module = {
    PyModuleDef_HEAD_INIT,
    "dashem_native",
    "Low-level C extension for em-dash removal",
    -1,
    DashemMethods
};

/**
 * Module initialization
 */
PyMODINIT_FUNC PyInit_dashem_native(void) {
    return PyModule_Create(&dashem_native_module);
}
