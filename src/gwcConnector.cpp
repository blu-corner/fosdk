#include "gwcConnector.h"

#include <dl.h>
#include <sstream>


namespace neueda
{
gwcConnector*
gwcConnectorFactory::get (logger* log, const std::string& type, const neueda::properties& props)
{
    std::stringstream lib;
#ifndef WIN32
    lib << "libgwc" << type << SBF_SHLIB_SUFFIX;
#else
    lib << "gwc" << type << ".dll";
#endif
    dl_handle handle = dl_open (lib.str ().c_str ());

    if (handle == NULL) {
        log->err ("%s", dl_error ());
        log->fatal ("unable to load connector [%s]", type.c_str ());
    }

    gwcConnector::getConnector g = (gwcConnector::getConnector)dl_symbol (handle, "getConnector");
    if (g == NULL)
        log->fatal ("can't find getConnector function in %s", lib.str ().c_str ());

    gwcConnector* connector = g (log, props);
    if (connector == NULL)
        log->fatal ("failed to create connector");

    return connector;
}
}
