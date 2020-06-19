/*
 * Copyright 2014-2018 Neueda Ltd.
 */

#pragma once

#include "fileLogHandler.h"
#include <string>

using namespace std;

namespace neueda
{

class msgWriter
{
public:

    msgWriter (const std::string& path,
               const size_t limit,
               const int fileCount);
    
    ~msgWriter ();

    bool setup (string& err);

    void teardown ();

    void write (void* data, size_t len);

private:
    void roll ();

    FILE*               mFile;
    std::string         mPath;
    size_t              mSize;
    size_t              mSizeLimit;
    int                 mCountLimit;
    int                 mCount;
};

};
