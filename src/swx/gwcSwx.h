#pragma once
/*
 * SWX connector 
 */
#include "gwcSoupBin.h"
#include "swxCodec.h"


class gwcSwx : public gwcSoupBin
{
public:
    gwcSwx (neueda::logger* log);
    virtual ~gwcSwx ();

    bool sendOrder (gwcOrder& order);
    bool sendOrder (cdr& order);    

    bool sendCancel (gwcOrder& cancel);
    bool sendCancel (cdr& cancel);

    bool sendModify (gwcOrder& modify);
    bool sendModify (cdr& modify);

    bool sendMsg (cdr& msg);
    
protected:
    bool mapOrderFields (gwcOrder& order);
    neueda::codec& getCodec ();
    void handleSequencedMessage (cdr& msg);
    void handleUnsequencedMessage (cdr& msg);

private:
    neueda::swxCodec mCodec;
};

