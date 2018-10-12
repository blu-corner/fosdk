#include "sbfPerfCounter.h"

#include <iostream>
#include <fstream>
#include <vector>
#include <stdexcept>
#include <cstdlib>
#include <sys/time.h>


namespace benchmark
{
    enum TIMESTAMP_TYPE {
        RDTSC, // cpu perf counter
        GTOD   // gettimeofday
    };
    
class TimestampFactory {
public:

    TimestampFactory(TIMESTAMP_TYPE type)
        : mType (type),
          mCpuFreq (sbfPerfCounter_frequency())
    {
    }

    uint64_t getTimestamp () const
    {
        switch (mType)
        {
        case RDTSC:
            return sbfPerfCounter_get ();
        case GTOD:
        {
            ::timeval tv;
            gettimeofday (&tv, NULL);

            return tv.tv_sec * (uint64_t)1000000 + tv.tv_usec;
        }   
        }
        
        return -1;
    }

    TIMESTAMP_TYPE getTimestampType () const { return mType; }

    static std::string timestampMethodToString (TIMESTAMP_TYPE type)
    {
        switch (type)
        {
        case RDTSC:
            return "rdtsc";
        case GTOD:
            return "gettimeofday";
        }
        
        return "unknown";
    }

    uint64_t getCpuFrequency () const { return mCpuFreq; }

private:
    TIMESTAMP_TYPE mType;
    uint64_t mCpuFreq;
};


class TimestampFile {
public:
    TimestampFile (size_t capacity, TimestampFactory& tsFactory)
        : mCapacity (capacity),
          mOffs (0),
          mTsFactory (tsFactory)
    {
        mSamples.resize (mCapacity);
    }

    void pushSample ()
    {
        uint64_t ts = mTsFactory.getTimestamp ();
        if (mOffs >= mCapacity) {
            mSamples.push_back (ts);
        }
        else {
            mSamples[mOffs++] = ts;
        }
    }

    size_t numSamples () const
    {
        return  (mOffs < mCapacity) ? mOffs : mSamples.size ();
    }

    void saveToDisk (const std::string& path)
    {
        TIMESTAMP_TYPE methodType = mTsFactory.getTimestampType ();
        uint64_t cpuFrequency = mTsFactory.getCpuFrequency ();
        std::string methodTypeString = TimestampFactory::timestampMethodToString (methodType);

        // <num-samples:uint>
        // <cpu_frequency:uint64_t>
        // <method_string_length:uint>
        // <method_string:str>
        // <num_padding_bytes:uint>
        // <padding-bytes:bytes*n>

        FILE* fd = fopen (path.c_str (), "wb");

        size_t nSamples = numSamples ();
        size_t methodStringLength = methodTypeString.size ();
        
        fwrite (&nSamples, sizeof(unsigned int), 1, fd);
        fwrite (&cpuFrequency, sizeof(uint64_t), 1, fd);
        fwrite (&methodStringLength, sizeof(unsigned int), 1, fd);
        fwrite (methodTypeString.c_str (), sizeof(char), methodTypeString.size (), fd);

        size_t numPadding = sizeof(int) + methodTypeString.size () % sizeof(int);
        unsigned char padding[numPadding];
        memset (padding, 0, numPadding);

        fwrite (&numPadding, sizeof(unsigned int), 1, fd);
        fwrite (padding, sizeof(char), numPadding, fd);
        fwrite (&(mSamples[0]), sizeof(uint64_t), mSamples.size (), fd);
        
        fclose (fd);
    }
    
private:
    size_t mCapacity;
    size_t mOffs;
    TimestampFactory& mTsFactory;
    std::vector<uint64_t> mSamples;
};
}
