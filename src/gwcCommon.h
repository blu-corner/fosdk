#pragma once
/*
 * Common defines for gateway connector 
 */

#include "cdr.h"
#include <string>

namespace neueda
{

typedef enum
{
    GWC_SIDE_BUY = '1',
    GWC_SIDE_SELL = '2',
    GWC_SIDE_BUY_MINUS = '3',
    GWC_SIDE_SELL_PLUS = '4',
    GWC_SIDE_SELL_SHORT = '5',
    GWC_SIDE_SELL_SHORT_EXEMPT = '6',
    GWC_SIDE_UNDISCLOSED = '7',
    GWC_SIDE_CROSS = '8',
    GWC_SIDE_CROSS_SHORT = '9',
    GWC_SIDE_CROSS_SHORT_EXEMPT = 'A',
    GWC_SIDE_AS_DEFINED = 'B',
    GWC_SIDE_OPPOSITE = 'C',
    GWC_SIDE_SUBSCRIBE = 'D',
    GWC_SIDE_REDEEM = 'E',
    GWC_SIDE_LEND = 'F',
    GWC_SIDE_BORROW = 'G',
    GWC_SIDE_SELL_UNDISCLOSED = 'H',
} gwcSide;

typedef enum
{
    GWC_ORDER_TYPE_MARKET = '1',
    GWC_ORDER_TYPE_LIMIT = '2',
    GWC_ORDER_TYPE_STOP = '3',
    GWC_ORDER_TYPE_STOP_LIMIT = '4',
    GWC_ORDER_TYPE_MARKET_ON_CLOSE = '5',
    GWC_ORDER_TYPE_WITH_OR_WITHOUT = '6',
    GWC_ORDER_TYPE_LIMIT_OR_BETTER = '7',
    GWC_ORDER_TYPE_LIMIT_WITH_OR_WITHOUT = '8',
    GWC_ORDER_TYPE_ON_BASIS = '9',
    GWC_ORDER_TYPE_ON_CLOSE = 'A',
    GWC_ORDER_TYPE_LIMIT_ON_CLOSE = 'B',
    GWC_ORDER_TYPE_FOREX = 'C',
    GWC_ORDER_TYPE_PREVIOUSLY_QUOTED = 'D',
    GWC_ORDER_TYPE_PREVIOUSLY_INDICATED = 'E',
    GWC_ORDER_TYPE_PEGGED = 'P',
} gwcOrderType;

typedef enum
{
    GWC_TIF_DAY = '0',
    GWC_TIF_GTC = '1',
    GWC_TIF_OPG = '2',
    GWC_TIF_IOC = '3',
    GWC_TIF_FOK = '4',
    GWC_TIF_GTX = '5',
    GWC_TIF_GTD = '6',
    GWC_TIF_ATC = '7',
    GWC_TIF_GTT = '8',
    GWC_TIF_CPX = '9',
    GWC_TIF_GFA = 'A',
    GWC_TIF_GFX = 'B',
    GWC_TIF_GFS = 'C',
} gwcTif;

class gwcOrder : public cdr
{
public:
    void setPrice (double price)
    {
        mPriceSet = true;
        mPrice = price;
    }

    void setQty (uint64_t qty)
    {
        mQtySet = true;
        mQty = qty;
    }

    void setSide (gwcSide side)
    {
        mSideSet = true;
        mSide = side;
    }

    void setOrderType (gwcOrderType type)
    {
        mOrderTypeSet = true;
        mOrderType = type;
    }

    void setTif (gwcTif tif)
    {
        mTifSet = true;
        mTif = tif;
    }

    double       mPrice;
    bool         mPriceSet;
    uint64_t     mQty;
    bool         mQtySet;
    gwcSide      mSide;
    bool         mSideSet;
    gwcOrderType mOrderType;
    bool         mOrderTypeSet;
    gwcTif       mTif;
    bool         mTifSet;
};
}
