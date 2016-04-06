#include <boost/histogram/axis.hpp>
#include <boost/histogram/nhistogram.hpp>
#include <boost/python.hpp>
#include <boost/python/raw_function.hpp>
#include <boost/foreach.hpp>
#include <boost/shared_ptr.hpp>
#include "serialization_suite.hpp"
#ifdef USE_NUMPY
# define NO_IMPORT_ARRAY
# define PY_ARRAY_UNIQUE_SYMBOL boost_histogram_ARRAY_API
# define NPY_NO_DEPRECATED_API NPY_1_7_API_VERSION
# include <numpy/arrayobject.h>
#endif

namespace boost {
namespace histogram {

python::object
nhistogram_init(python::tuple args, python::dict kwargs) {
    using namespace python;
    using python::tuple;

    object self = args[0];
    object pyinit = self.attr("__init__");

    if (kwargs) {
        PyErr_SetString(PyExc_TypeError, "no keyword arguments allowed");
        throw_error_already_set();        
    }

    // normal constructor
    axes_type axes;
    for (unsigned i = 1, n = len(args); i < n; ++i) {
        object pa = args[i];
        extract<regular_axis> er(pa);
        if (er.check()) { axes.push_back(er()); continue; }
        extract<polar_axis> ep(pa);
        if (ep.check()) { axes.push_back(ep()); continue; }
        extract<variable_axis> ev(pa);
        if (ev.check()) { axes.push_back(ev()); continue; }
        extract<category_axis> ec(pa);
        if (ec.check()) { axes.push_back(ec()); continue; }
        extract<integer_axis> ei(pa);
        if (ei.check()) { axes.push_back(ei()); continue; }
        PyErr_SetString(PyExc_TypeError, "require an axis object");
        throw_error_already_set();
    }
    return pyinit(axes);
}

python::dict
nhistogram_array_interface(nhistogram& self) {
    using namespace python;
    using python::tuple;
    using python::make_tuple;

    list shape;
    for (unsigned i = 0; i < self.dim(); ++i)
        shape.append(self.shape(i));
    dict d;
    d["shape"] = tuple(shape);
    d["typestr"] = str("<u") + str(self.data().depth());
    d["data"] = make_tuple((long long)(self.data().buffer()), false);
    return d;
}

python::object
nhistogram_fill(python::tuple args, python::dict kwargs) {
    using namespace python;

    const unsigned nargs = len(args);
    nhistogram& self = extract<nhistogram&>(args[0]);

#ifdef USE_NUMPY
    if (nargs == 2) {
        object o = args[1];
        if (PySequence_Check(o.ptr())) {
            PyArrayObject* a = (PyArrayObject*)
                PyArray_FROM_OTF(o.ptr(), NPY_DOUBLE, NPY_ARRAY_IN_ARRAY);
            if (!a) {
                PyErr_SetString(PyExc_ValueError, "could not convert sequence into array");
                throw_error_already_set();
            }

            npy_intp* dims = PyArray_DIMS(a);
            switch (PyArray_NDIM(a)) {
                case 1: 
                if (self.dim() > 1) {
                    PyErr_SetString(PyExc_ValueError, "array has to be two-dimensional");
                    throw_error_already_set();
                }
                break;
                case 2:
                if (self.dim() != dims[1])
                {
                    PyErr_SetString(PyExc_ValueError, "size of second dimension does not match");
                    throw_error_already_set();
                }
                break;
                default:
                    PyErr_SetString(PyExc_ValueError, "array has wrong dimension");
                    throw_error_already_set();
            }

            for (unsigned i = 0; i < dims[0]; ++i) {
                double* v = (double*)PyArray_GETPTR1(a, i);
                self.fill(v);
            }

            Py_DECREF(a);
            return object();
        }
    }
#endif

    const unsigned dim = nargs - 1;
    if (dim != self.dim()) {
        PyErr_SetString(PyExc_TypeError, "wrong number of arguments");
        throw_error_already_set();            
    }

    if (kwargs) {
        PyErr_SetString(PyExc_TypeError, "no keyword arguments allowed");
        throw_error_already_set();        
    }

    double v[BOOST_HISTOGRAM_AXIS_LIMIT];
    for (unsigned i = 0; i < dim; ++i)
        v[i] = extract<double>(args[1 + i]);
    self.fill(v);
    return object();
}

unsigned
nhistogram_depth(const nhistogram& self) {
    return self.data().depth();
}

uint64_t
nhistogram_getitem(const nhistogram& self, python::object oidx) {
    using namespace python;

    if (self.dim() == 1)
        return self(extract<int>(oidx)());

    const unsigned dim = len(oidx);
    if (dim != self.dim()) {
        PyErr_SetString(PyExc_RuntimeError, "wrong number of arguments");
        throw_error_already_set();            
    }

    int idx[BOOST_HISTOGRAM_AXIS_LIMIT];
    for (unsigned i = 0; i < dim; ++i)
        idx[i] = extract<int>(oidx[i]);

    return self(idx);
}

void register_nhistogram()
{
    using namespace python;

    class_<
      nhistogram, bases<histogram_base>,
      shared_ptr<nhistogram>
    >("nhistogram", no_init)
        .def("__init__", raw_function(nhistogram_init))
        // shadowed C++ ctors
        .def(init<const axes_type&>())
        .add_property("__array_interface__", nhistogram_array_interface)
        .def("fill", raw_function(nhistogram_fill))
        .add_property("depth", nhistogram_depth)
        .add_property("sum", &nhistogram::sum)
        .def("__getitem__", nhistogram_getitem)
        .def(self == self)
        .def(self += self)
        .def(self + self)
        .def_pickle(serialization_suite<nhistogram>())
        ;
}

}
}