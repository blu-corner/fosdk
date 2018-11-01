// binary interfaces
%include "cstring.i"

%pragma(java) jniclasscode=%{
     // jniclasscode pragma code: Static block so that the JNI class loads the C++ DLL/shared object when the class is loaded
     static {
         try {
             System.loadLibrary("Fosdk");
         } catch (UnsatisfiedLinkError e) {
             System.err.println("Native code library failed to load. See the chapter on Dynamic Linking Problems in the SWIG Java documentation for help.\n" + e);
             System.exit(1);
         }
     }
%}

%include "exception.i"
%exception {
    try {
        $action
    } catch (std::exception &e) {
        std::string s("codec-error: "), s2(e.what());
        s = s + s2;
        SWIG_exception(SWIG_RuntimeError, s.c_str());
    }
}

 // backtraces in delegates
%feature("director:except") %{
    jthrowable $error = jenv->ExceptionOccurred();
    if ($error) {
        jenv->ExceptionClear();
        $directorthrowshandlers
            throw Swig::DirectorException(jenv, $error);
    }
%}

%apply (char *STRING, int LENGTH) { (void* data, size_t len) };
%apply (char *STRING, int LENGTH) { (const void* data, size_t len) };
%apply (char *STRING, int LENGTH) { (const void* ptr, size_t len) };

%pragma(java) moduleimports=%{
import com.neueda.properties.Properties;
import com.neueda.logger.Logger;
import com.neueda.codec.Buffer;
import com.neueda.cdr.Cdr;
%}

%pragma(java) jniclassimports=%{
import com.neueda.properties.Properties;
import com.neueda.logger.Logger;
import com.neueda.codec.Buffer;
import com.neueda.cdr.Cdr;
%}

%typemap(javaimports) SWIGTYPE %{
import com.neueda.properties.Properties;
import com.neueda.logger.Logger;
import com.neueda.codec.Buffer;
import com.neueda.cdr.Cdr;

%}

%typemap("javapackage") neueda::cdr, neueda::cdr*, neueda::cdr & "com.neueda.cdr";

SWIG_JAVABODY_PROXY(public, public, SWIGTYPE)
SWIG_JAVABODY_TYPEWRAPPER(public, public, public, SWIGTYPE)

%include "fosdk.i"
