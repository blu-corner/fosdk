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
    bool sendCancel (cdr& cancel);
    bool sendModify (cdr& modify);
    bool sendMsg (cdr& msg);
    
protected:
    neueda::codec& getCodec ();
    void handleSequencedMessage (cdr& msg);
    void handleUnsequencedMessage (cdr& msg);

private:
    neueda::swxCodec mCodec;
};

