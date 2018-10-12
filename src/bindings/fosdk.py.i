// binary interfaces
%include "cstring.i"

%feature("autodoc");

%include "exception.i"
%exception {
    try {
        $action
    } catch (std::exception &e) {
        std::string s("fosdk-error: "), s2(e.what());
        s = s + s2;
        SWIG_exception(SWIG_RuntimeError, s.c_str());
    }
}

%apply (char *STRING, int LENGTH) { (void* data, size_t len) };
%apply (char *STRING, int LENGTH) { (const void* data, size_t len) };

// get backtraces on director exceptions instead of silent crashes
%feature("director:except") {
    if ($error != NULL)
    {
        PyObject *ptype, *pvalue, *ptraceback;
        PyErr_Fetch (&ptype, &pvalue, &ptraceback);
        PyErr_Restore (ptype, pvalue, ptraceback);
        PyErr_Print ();
        Py_Exit (1);
    }
 }

%include "fosdk.i"
