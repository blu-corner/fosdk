#include "msgWriter.h"

#include <cstdio>
#include <iostream>
#include <sstream>
#include <cerrno>
#include <cstring>

#include <sys/types.h>
#include <sys/stat.h>

namespace neueda
{

msgWriter::msgWriter (const string& path,
                      const size_t limit,
                      const int fileCount) :
    mPath (path),
    mSizeLimit (limit),
    mCountLimit (fileCount)
{
}

bool
msgWriter::setup (string& err)
{
    errno = 0;

    FILE* f = fopen (mPath.c_str (), "w");
    if (f == NULL)
    {
        err.assign (strerror (errno));
        return false;
    }

    // no buffering
    setbuf (f, NULL);

    errno = 0;
    // get current size
    struct stat sb;
    if (fstat (fileno (f), &sb) != 0)
    {
        err.assign (strerror (errno));
        fclose (f);
        return false;
    }

    mFile = f;
    mSize = sb.st_size;

    return true;
}

void
msgWriter::teardown ()
{
    if (mFile != NULL)
    {
        fclose (mFile);
        mFile = NULL;
    }
}

void
msgWriter::roll ()
{
    string errorMessage;

    mCount++;
    if (mCountLimit > 0 && mCount >= mCountLimit)
        mCount = 1;

    stringstream to;
    stringstream from;
    for (int i = mCount - 1; i >= 0; i--)
    {
        to.str ("");
        from.str ("");

        to << mPath << "." << i + 1;

        if (i == 0)
            from << mPath;
        else
            from << mPath << "." << i;

        if (rename (from.str ().c_str (), to.str ().c_str ()) != 0)
        {
            errorMessage.assign ("failed to rename");
            return;
        }
    }

    fclose (mFile);
    if (!setup (errorMessage))
        return;
}

void
msgWriter::write (void* data, size_t len)
{
    if (mSizeLimit != 0 && mSize + (len + 1) > mSizeLimit)
        roll ();

    mSize += fwrite (data, len, 1, mFile);
    mSize += fwrite ("\n", 1, 1, mFile);
}

}
