/*
 * Copyright 2014-2018 Neueda
 */
#pragma once

namespace neueda {

    class Buffer {
    public:
        Buffer (void* bytes, size_t length, bool owned)
            : mBytes (bytes),
              mLength (length),
              mOwned (owned)
        {
        }

        ~Buffer ()
        {
            if (mOwned)
                free (mBytes);
        }

        void* getPointer () const { return mBytes; }

        void setRaw (void* bytes, size_t length, bool owned)
        {
            mBytes  = bytes;
            mLength = length;
            mOwned  = owned;
        }

        size_t getLength () const { return mLength; }

    private:
        void*    mBytes;
        size_t   mLength;
        bool     mOwned;
    };

    class DecodeResults
    {
    public:
        DecodeResults (cdr* obj, int used, bool owned)
            : mCdr (obj),
              mUsed (used),
              mOwned (owned)
        {
        }

        ~DecodeResults ()
        {
            if (mOwned)
                delete mCdr;
        }

        const cdr* getCdr() const { return mCdr; }
        const int getUsed() const { return mUsed; }

    private:
        cdr*   mCdr;
        int    mUsed;
        bool   mOwned;
    };
}
