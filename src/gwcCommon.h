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
    GWC_SIDE_BUY,
    GWC_SIDE_SELL,
    GWC_SIDE_SELL_SHORT,
    GWC_SIDE_SELL_SHORT_EXEMPT,
} gwcSide;

typedef enum
{
    GWC_ORDER_TYPE_MARKET,
    GWC_ORDER_TYPE_LIMIT,
    GWC_ORDER_TYPE_PEGGED,
    GWC_ORDER_TYPE_STOP,
    GWC_ORDER_TYPE_STOP_LIMIT,
} gwcOrderType;

typedef enum
{
    GWC_TIF_DAY,
    GWC_TIF_IOC,
    GWC_TIF_FOK,
    GWC_TIF_OPG,
    GWC_TIF_GTD,
    GWC_TIF_GTT,
    GWC_TIF_ATC,
    GWC_TIF_CPX,
    GWC_TIF_GFA,
    GWC_TIF_GFX,
    GWC_TIF_GFS,
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
