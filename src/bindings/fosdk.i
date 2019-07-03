%module(directors="1", thread="1") Fosdk

%{
#include <stdexcept>
#include <sstream>
#include <string>

#include "cdr.h"
#include "gwcCommon.h"
#include "gwcConnector.h"

#include "bindings/codecBuffer.h"
%}

%include "std_string.i"
%include "stdint.i"
%include "std_vector.i"
%include "cdata.i"
%include "typemaps.i"

// macros
%define __attribute__(x)
%enddef

%rename(Cdr) neueda::cdr;
%rename(Properties) neueda::properties;
%rename(Logger) neueda::logger;
%rename(Codec) neueda::codec;

%import(module="properties") "properties.h"
%import(module="logger") "logger.h"
%import(module="cdr") "cdr.h"
%import(module="codec") "codec.h"


// directors
%feature("director") gwcSessionCallbacks;
%feature("director") gwcMessageCallbacks;
%feature("director") gwcConnector;

%extend neueda::gwcConnector {
    bool sendBuffer(neueda::Buffer* buffer)
    {
        return self->sendRaw ((void*)buffer->getPointer (),
                              buffer->getLength ());
    }
}

// include
%include "gwcCommon.h"
%include "gwcConnector.h"
