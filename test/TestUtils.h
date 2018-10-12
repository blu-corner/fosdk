#pragma once

#include <gtest/gtest.h>
#include <gmock/gmock.h>

#include "SbfTcpConnection.hpp"
#include "gwcConnector.h"


class MockSessionCallbacks : public neueda::gwcSessionCallbacks
{
public:
    MOCK_METHOD0 (onConnected, void());

    MOCK_METHOD1 (onLoggingOn, void(cdr& msg));

    MOCK_METHOD1 (onError, bool(const std::string& error));

    MOCK_METHOD2 (onLoggedOn, void(uint64_t seqno, 
                                   const cdr& msg));

    MOCK_METHOD2 (onTraderLogonOn, void(std::string traderId, 
                                        const cdr& msg));

    MOCK_METHOD2 (onLoggedOff, void(uint64_t seqno, 
                                    const cdr& msg));

    MOCK_METHOD2 (onGap, void(uint64_t expected, 
                              uint64_t recieved));
};

class MockMessageCallbacks : public neueda::gwcMessageCallbacks
{
public:

    MOCK_METHOD2 (onAdmin, void(uint64_t seqno, 
                                const cdr& msg));

    MOCK_METHOD2 (onOrderAck, void(uint64_t seqno, 
                                   const cdr& msg));

    MOCK_METHOD2 (onOrderRejected, void(uint64_t seqno, 
                                        const cdr& msg));

    MOCK_METHOD2 (onOrderDone, void(uint64_t seqno, 
                                    const cdr& msg));

    MOCK_METHOD2 (onOrderFill, void(uint64_t seqno, 
                                    const cdr& msg));

    MOCK_METHOD2 (onModifyAck, void(uint64_t seqno, 
                                    const cdr& msg));

    MOCK_METHOD2 (onModifyRejected, void(uint64_t seqno, 
                                         const cdr& msg));

    MOCK_METHOD2 (onCancelRejected, void(uint64_t seqno, 
                                         const cdr& msg));

    MOCK_METHOD2 (onMsg, void(uint64_t seqno, 
                              const cdr& msg));

    MOCK_METHOD3 (onRawMsg, void(uint64_t seqno, 
                                 const void* ptr, 
                                 size_t len));
};

class MockSbfTcpConnection : public neueda::SbfTcpConnection
{
public:
    MockSbfTcpConnection (sbfLog log,
                          struct sbfMwThreadImpl* thread,
                          struct sbfQueueImpl* queue,
                          sbfTcpConnectionAddress* address,
                          bool isUnix,
                          bool disableNagles,
                          SbfTcpConnectionDelegate* delegate)
        : SbfTcpConnection (log,
                            thread,
                            queue,
                            address,
                            isUnix,
                            disableNagles,
                            delegate)
    { }

    MOCK_METHOD0 (connect, bool());
    
    MOCK_METHOD2 (send, void(const void* data, size_t size));
};
