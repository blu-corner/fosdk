%typemap(throws, canthrow=1) std::runtime_error {
    SWIG_CSharpSetPendingExceptionArgument(SWIG_CSharpApplicationException, $1.what(), NULL);
    return $null;
}

%typemap(csimports) SWIGTYPE %{
using Neueda.Properties;
using Neueda.Log;
using Neueda.Cdr;
using Neueda.Codecs;
%}

SWIG_CSBODY_PROXY(public, public, SWIGTYPE)
SWIG_CSBODY_TYPEWRAPPER(public, public, public, SWIGTYPE)

%include "typemaps.i";

//%apply void *VOID_INT_PTR { void* }

%include "fosdk.i"
