#include "swxCodecConstants.h"
#include "gwcSwx.h"


extern "C" gwcConnector*
getConnector (neueda::logger* log, const neueda::properties& props)
{
    return new gwcSwx (log);
}

gwcSwx::gwcSwx (neueda::logger* log)
    : gwcSoupBin (log)
{
}

gwcSwx::~gwcSwx ()
{
}

neueda::codec&
gwcSwx::getCodec ()
{
    return mCodec;
}

bool
gwcSwx::sendOrder (gwcOrder& order)
{
    if (order.mOrderTypeSet)
    {
        switch (order.mOrderType)
        {
        case GWC_ORDER_TYPE_MARKET:
            order.setInteger (OrderPrice, 0x7FFFFFFF);
            break;
        case GWC_ORDER_TYPE_LIMIT:
            break;
        default:
            mLog->err ("invalid order type");
            return false;
        }
    }

    if (order.mPriceSet)
        order.setInteger (OrderPrice, order.mPrice);

    if (order.mQtySet)
        order.setInteger (OrderQuantity, order.mQty);

    if (order.mSideSet)
    {
        switch (order.mSide)
        {
        case GWC_SIDE_BUY:
            order.setString (OrderVerb, "%c", SWX_ORDERVERB_BUY);
            break;
        case GWC_SIDE_SELL:
            order.setString (OrderVerb, "%c", SWX_ORDERVERB_SELL);
            break;
        default:
            mLog->err ("invalid side value");
            return false;
        }
    }

    if (order.mTifSet)
    {
        switch (order.mTif)
        {
        case GWC_TIF_IOC:
            order.setInteger (TimeInForce, SWX_TIMEINFORCE_IMMEDIATE);
            break;
        case GWC_TIF_GTT:
            order.setInteger (TimeInForce, SWX_TIMEINFORCE_SESSIONORDEREXPIRESATCLOSE);
            break;
        case GWC_TIF_OPG:
            order.setInteger (TimeInForce, SWX_TIMEINFORCE_SESSIONORDEREXPIRESATTHEOPENING);
            break;
        case GWC_TIF_DAY:
            order.setInteger (TimeInForce, SWX_TIMEINFORCE_DAYORDEREXPIRESATENTEROFPOSTTRADING);
            break;
        default:
            mLog->err ("unhandled tif value");
            return false;
        }
    }

    // downgrade to cdr so compiler picks correct method
    cdr& o = order;
    return sendOrder (o);
}

bool
gwcSwx::sendOrder (cdr& order)
{
    order.setString (MessageType, "%c", SWX_UNSEQUENCED_MESSAGE_TYPE);
    order.setString (Type, "%c", SWX_ENTER_ORDER_MESSAGE_TYPE);
    return sendMsg (order);
}

bool
gwcSwx::sendCancel (cdr& cancel)
{
    cancel.setString (MessageType, "%c", SWX_UNSEQUENCED_MESSAGE_TYPE);
    cancel.setString (Type, "%c", SWX_CANCEL_ORDER_MESSAGE_TYPE);
    return sendMsg (cancel);
}

bool
gwcSwx::sendModify (cdr& modify)
{
    modify.setString (MessageType, "%c", SWX_UNSEQUENCED_MESSAGE_TYPE);
    modify.setString (Type, "%c", SWX_REPLACE_ORDER_MESSAGE_TYPE);
    return sendMsg (modify);
}

void
gwcSwx::handleSequencedMessage (cdr& msg)
{
    string mTypeStr;
    bool ok = msg.getString (Type, mTypeStr);
    if (!ok)
    {
        mLog->err ("no type on sequenced message [%s]", msg.toString ().c_str ());
        return;
    }

    char mType = *(mTypeStr.c_str ());
    switch (mType)
    {
    case SWX_SYSTEM_EVENT_MESSAGE_TYPE:
        mMessageCbs->onAdmin (mSequenceNumber, msg);
        break;

    case SWX_ACCEPTED_MESSAGE_TYPE:
        mMessageCbs->onOrderAck (mSequenceNumber, msg);
        break;

    case SWX_REPLACED_MESSAGE_TYPE:
        mMessageCbs->onModifyAck (mSequenceNumber, msg);
        break;
        
    case SWX_CANCELLED_MESSAGE_TYPE:
        mMessageCbs->onOrderDone (mSequenceNumber, msg);
        break;

    case SWX_EXECUTED_ORDER_MESSAGE_TYPE:
        mMessageCbs->onOrderFill (mSequenceNumber, msg);
        break;

    case SWX_REJECTED_ORDER_MESSAGE_TYPE:
        mMessageCbs->onOrderRejected (mSequenceNumber, msg);
        break;

    case SWX_ORDER_PRIORITY_UPDATE_CHANGE_MESSAGE_TYPE:
    case SWX_BROKEN_TRADE_MESSAGE_TYPE:
        mMessageCbs->onMsg (mSequenceNumber, msg);
        break;
            
    default:
        mLog->err ("unable to handle sequenced message-type [%c]", mType);
        mLog->err ("%s", msg.toString ().c_str ());
        break;
    }
}

void
gwcSwx::handleUnsequencedMessage (cdr& msg)
{
    // pass to on message for now in-case this happens
    mMessageCbs->onMsg (mSequenceNumber, msg);
}

bool
gwcSwx::sendMsg (cdr& msg)
{
    char space[1024];
    size_t used;
    
    // use a codec from the stack gets around threading issues
    neueda::swxCodec codec;

    if (mState != GWC_CONNECTOR_READY)
    {
        mLog->warn ("gwc not ready to send messages");
        return false;
    }
    
    if (codec.encode (msg, space, sizeof space, used) != GW_CODEC_SUCCESS)
    {
        mLog->err ("failed to construct message [%s]", codec.getLastError ().c_str ());
        return false;
    }

    mConnection->send (space, used);
    return true;
}
