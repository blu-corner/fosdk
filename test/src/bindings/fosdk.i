%module(directors="1", thread="1") Fosdk

%{
#include <stdexcept>
#include <sstream>
#include <string>

#include "cdr.h"
#include "gwcCommon.h"
#include "gwcConnector.h"

#include "../../ext/codec/src/bindings/codecBuffer.h"
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

%import(module="Config") "properties.h"
%import(module="Log") "logger.h"
%import(module="CommonDataRepresentation") "cdr.h"
%import(module="Codecs") "codec.h"
%import(module="Codecs") "../../ext/codec/src/bindings/codecBuffer.h"

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
