/*
 * ClusterDiscoveryService.cpp
 *
 * Copyright (C) 2006 -2007 by Universitaet Stuttgart (VIS). 
 * Alle Rechte vorbehalten.
 */

#include "vislib/ClusterDiscoveryService.h"

#include "vislib/assert.h"
#include "vislib/IllegalParamException.h"
#include "vislib/SocketException.h"
#include "vislib/Trace.h"


/*
 * vislib::net::ClusterDiscoveryService::DEFAULT_PORT 
 */
const USHORT vislib::net::ClusterDiscoveryService::DEFAULT_PORT = 28181;


/*
 * vislib::net::ClusterDiscoveryService::MSG_TYPE_USER
 */
const UINT16 vislib::net::ClusterDiscoveryService::MSG_TYPE_USER = 16;


/*
 * vislib::net::ClusterDiscoveryService::ClusterDiscoveryService
 */
vislib::net::ClusterDiscoveryService::ClusterDiscoveryService(
        const StringA& name, const SocketAddress& responseAddr, 
        const IPAddress& bcastAddr, const USHORT bindPort, 
        const UINT requestInterval, const UINT cntResponseChances)
        : bcastAddr(SocketAddress::FAMILY_INET, bcastAddr, bindPort), 
        bindAddr(SocketAddress::FAMILY_INET, bindPort), 
        responseAddr(responseAddr), 
        cntResponseChances(cntResponseChances),
        requestInterval(requestInterval),
        name(name), 
        sender(NULL), 
        receiver(NULL), 
        receiverThread(NULL),
        senderThread(NULL){
    this->name.Truncate(MAX_NAME_LEN);

    this->peerNodes.Resize(0);  // TODO: Remove alloc crowbar!
}


/*
 * vislib::net::ClusterDiscoveryService::~ClusterDiscoveryService
 */
vislib::net::ClusterDiscoveryService::~ClusterDiscoveryService(void) {
    if (this->senderThread != NULL) {
        try {
            this->senderThread->Terminate(false);
            SAFE_DELETE(this->sender);
            SAFE_DELETE(this->senderThread);
        } catch (...) {
            TRACE(Trace::LEVEL_WARN, "The discovery sender thread could "
                "not be successfully terminated.\n");
        }
    }

    if (this->receiverThread != NULL) {
        try {
            this->receiverThread->Terminate(false);
            SAFE_DELETE(this->receiver);
            SAFE_DELETE(this->receiverThread);
        } catch (...) {
            TRACE(Trace::LEVEL_WARN, "The discovery receiver thread could "
                "not be successfully terminated.\n");
        }
    }
}


/*
 * vislib::net::ClusterDiscoveryService::AddListener
 */
void vislib::net::ClusterDiscoveryService::AddListener(
        ClusterDiscoveryListener *listener) {
    ASSERT(listener != NULL);

    this->critSect.Lock();
    if ((listener != NULL) && !this->listeners.Contains(listener)) {
        this->listeners.Append(listener);
    }
    this->critSect.Unlock();
}


/*
 * vislib::net::ClusterDiscoveryService::RemoveListener
 */
void vislib::net::ClusterDiscoveryService::RemoveListener(
        ClusterDiscoveryListener *listener) {
    ASSERT(listener != NULL);

    this->critSect.Lock();
    this->listeners.Remove(listener);
    this->critSect.Unlock();
}


/*
 * vislib::net::ClusterDiscoveryService::SendUserMessage
 */
UINT vislib::net::ClusterDiscoveryService::SendUserMessage(
        const UINT16 msgType, const void *msgBody, const SIZE_T msgSize) {
    Message msg;                // The datagram we are going to send.
    UINT retval = 0;            // Number of failed communication trials.

    this->prepareUserMessage(msg, msgType, msgBody, msgSize);
    ASSERT(this->userMsgSocket.IsValid());

    /* Send the message to all registered clients. */
    this->critSect.Lock();
    for (SIZE_T i = 0; i < this->peerNodes.Count(); i++) {
        try {
            this->userMsgSocket.Send(&msg, sizeof(Message), 
                this->peerNodes[i].discoveryAddr);
        } catch (SocketException e) {
            TRACE(Trace::LEVEL_WARN, "ClusterDiscoveryService could not send "
                "user message (\"%s\").\n", e.GetMsgA());
            retval++;
        }
    }
    this->critSect.Unlock();

    return retval;
}

/*
 * vislib::net::ClusterDiscoveryService::SendUserMessage
 */
UINT vislib::net::ClusterDiscoveryService::SendUserMessage(
        const SocketAddress& to, const UINT16 msgType, const void *msgBody, 
        const SIZE_T msgSize) {
    Message msg;                // The datagram we are going to send.
    INT_PTR idx = -1;           // Index of target node.

    this->prepareUserMessage(msg, msgType, msgBody, msgSize);
    ASSERT(this->userMsgSocket.IsValid());

    this->critSect.Lock();
    if ((idx = this->peerFromResponseAddr(to)) >= 0) {
        try {
            // TODO: Should avoid cast.
            this->userMsgSocket.Send(&msg, sizeof(Message), 
                this->peerNodes[static_cast<INT>(idx)].discoveryAddr);
        } catch (SocketException e) {
            TRACE(Trace::LEVEL_WARN, "ClusterDiscoveryService could not send "
                "user message (\"%s\").\n", e.GetMsgA());
            idx = -1;
        }
    }
    this->critSect.Unlock();

    return (idx >= 0) ? 0 : 1;
}


/*
 * vislib::net::ClusterDiscoveryService::Start
 */
void vislib::net::ClusterDiscoveryService::Start(void) {

    if (this->sender == NULL) {
        ASSERT(this->senderThread == NULL);
        ASSERT(this->receiver == NULL);
        ASSERT(this->receiverThread == NULL);

        /* Prepare the request sender thread. */
        this->sender = new Sender(*this);
        this->senderThread = new vislib::sys::Thread(this->sender);
        this->senderThread->Start();

        /* Prepare the receiver thread. */
        this->receiver = new Receiver(*this);
        this->receiverThread = new vislib::sys::Thread(this->receiver);
        this->receiverThread->Start();
    }
}


/*
 * vislib::net::ClusterDiscoveryService::Stop
 */
bool vislib::net::ClusterDiscoveryService::Stop(void) {
    try {
        // Note: Receiver must be stopped first, in order to prevent shutdown
        // deadlock when waiting for the last message.
        this->receiverThread->TryTerminate(false);
        this->senderThread->Terminate(false);

        return true;
    } catch (sys::SystemException e) {
        TRACE(Trace::LEVEL_ERROR, "Stopping discovery threads failed. The "
            "error code is %d (\"%s\").\n", e.GetErrorCode(), e.GetMsgA());
        return false;
    }
}


////////////////////////////////////////////////////////////////////////////////
// Begin of nested class Sender

/*
 * vislib::net::ClusterDiscoveryService::Sender::Sender
 */
vislib::net::ClusterDiscoveryService::Sender::Sender(
        ClusterDiscoveryService& cds) : cds(cds), isRunning(true) {
}


/*
 * vislib::net::ClusterDiscoveryService::Sender::~Sender
 */
vislib::net::ClusterDiscoveryService::Sender::~Sender(void) {
}


/*
 * vislib::net::ClusterDiscoveryService::Sender::Run
 */
DWORD vislib::net::ClusterDiscoveryService::Sender::Run(
        void *reserved) {
    Message request;        // The UDP datagram we send.
    Socket socket;          // The socket used for the broadcast.

    // Assert expected memory layout of messages.
    ASSERT(sizeof(request) == MAX_USER_DATA + 4);
    ASSERT(reinterpret_cast<BYTE *>(&(request.senderBody)) 
       == reinterpret_cast<BYTE *>(&request) + 4);

    /* Prepare the socket. */
    try {
        Socket::Startup();
        socket.Create(Socket::FAMILY_INET, Socket::TYPE_DGRAM, 
            Socket::PROTOCOL_UDP);
        socket.SetBroadcast(true);
    } catch (SocketException e) {
        TRACE(Trace::LEVEL_ERROR, "Discovery sender thread could not "
            "create its. The error code is %d (\"%s\").\n", e.GetErrorCode(),
            e.GetMsgA());
        return 1;
    }

    /* Prepare our request for multiple broadcasting. */
    request.magicNumber = MAGIC_NUMBER;
    request.msgType = MSG_TYPE_IAMHERE;
    request.senderBody.sockAddr = this->cds.responseAddr;
#if (_MSC_VER >= 1400)
    ::strncpy_s(request.senderBody.name, MAX_NAME_LEN, this->cds.name, 
        MAX_NAME_LEN);
#else /* (_MSC_VER >= 1400) */
    ::strncpy(request.senderBody.name, this->cds.name.PeekBuffer(), 
        MAX_NAME_LEN);
#endif /* (_MSC_VER >= 1400) */

    TRACE(Trace::LEVEL_INFO, "The discovery sender thread is starting ...\n");

    while (this->isRunning) {
        try {    
			/* Broadcast request. */
            this->cds.prepareRequest();
            socket.Send(&request, sizeof(Message), this->cds.bcastAddr);
            TRACE(Trace::LEVEL_INFO, "Discovery service sent MSG_TYPE_IAMHERE "
                "to %s.\n", this->cds.bcastAddr.ToStringA().PeekBuffer());

            sys::Thread::Sleep(this->cds.requestInterval);
        } catch (SocketException e) {
            TRACE(Trace::LEVEL_WARN, "A socket error occurred in the "
                "discovery sender thread. The error code is %d (\"%s\").\n",
                e.GetErrorCode(), e.GetMsgA());
        } catch (...) {
            TRACE(Trace::LEVEL_ERROR, "The discovery sender caught an "
                "unexpected exception.\n");
            return 2;
        }
    } /* end while (this->isRunning) */

    /* Clean up. */
    try {
  		/* Now inform all other nodes, that we are out. */
        request.msgType = MSG_TYPE_SAYONARA;
        socket.Send(&request, sizeof(Message), this->cds.bcastAddr);
        TRACE(Trace::LEVEL_INFO, "Discovery service sent MSG_TYPE_SAYONARA to "
            "%s.\n", this->cds.bcastAddr.ToStringA().PeekBuffer());
        
        Socket::Cleanup();
    } catch (SocketException e) {
        TRACE(Trace::LEVEL_ERROR, "Socket cleanup failed in the discovery "
            "request thread. The error code is %d (\"%s\").\n", 
            e.GetErrorCode(), e.GetMsgA());
    }

    return 0;
}


/*
 * vislib::net::ClusterDiscoveryService::Sender::Terminate
 */
bool vislib::net::ClusterDiscoveryService::Sender::Terminate(void) {
    // TODO: Should perhaps be protected by crit sect.
    this->isRunning = false;
    return true;
}

// End of nested class Sender
////////////////////////////////////////////////////////////////////////////////


////////////////////////////////////////////////////////////////////////////////
// Begin of nested class Receiver

/*
 * vislib::net::ClusterDiscoveryService::Receiver::Receiver
 */
vislib::net::ClusterDiscoveryService::Receiver::Receiver(
        ClusterDiscoveryService& cds) : cds(cds), isRunning(true) {
}


/*
 * vislib::net::ClusterDiscoveryService::Receiver::~Receiver
 */
vislib::net::ClusterDiscoveryService::Receiver::~Receiver(void) {
}


/*
 * vislib::net::ClusterDiscoveryService::Receiver::Run
 */
DWORD vislib::net::ClusterDiscoveryService::Receiver::Run(
        void *reserved) {
    SocketAddress peerAddr;     // Receives address of UDP communication peer.
    PeerNode peerNode;          // The peer node to register in our list.
    Message request;            // Receives the request messages.
    Socket socket;              // The datagram socket that is listening.

    // Assert expected message memory layout.
    ASSERT(sizeof(request) == MAX_USER_DATA + 4);
    ASSERT(reinterpret_cast<BYTE *>(&(request.senderBody)) 
       == reinterpret_cast<BYTE *>(&request) + 4);

    /* 
     * Prepare a datagram socket listening for requests on the specified 
     * adapter and port. 
     */
    try {
        Socket::Startup();
        socket.Create(Socket::FAMILY_INET, Socket::TYPE_DGRAM, 
            Socket::PROTOCOL_UDP);
        socket.SetBroadcast(true);
        socket.Bind(this->cds.bindAddr);
    } catch (SocketException e) {
        TRACE(Trace::LEVEL_ERROR, "Discovery receiver thread could not "
            "create its socket and bind it to the requested address. The "
            "error code is %d (\"%s\").\n", e.GetErrorCode(), e.GetMsgA());
        return 1;
    }

    /* Register myself as known node first. */
    peerNode.address = this->cds.responseAddr;
    peerNode.discoveryAddr = SocketAddress(this->cds.responseAddr, 
        this->cds.bindAddr.GetPort());  // TODO: This is hugly.
    peerNode.cntResponseChances = this->cds.cntResponseChances + 1;
    this->cds.addPeerNode(peerNode);

    TRACE(Trace::LEVEL_INFO, "The discovery receiver thread is starting ...\n");

    while (this->isRunning) {
        try {

            /* Wait for next message. */
            socket.Receive(&request, sizeof(Message), peerAddr);

            if (request.magicNumber == MAGIC_NUMBER) {
                /* Message OK, look for its content. */

                if ((request.msgType == MSG_TYPE_IAMHERE) 
                        && (this->cds.name.Compare(request.senderBody.name))) {
                    /* Got a discovery request for own cluster. */
                    TRACE(Trace::LEVEL_INFO, "Discovery service received "
                        "MSG_TYPE_IAMHERE from %s.\n", 
                        peerAddr.ToStringA().PeekBuffer());
                    
                    /* Add peer to local list, if not yet known. */
                    peerNode.address = request.senderBody.sockAddr;
                    this->cds.addPeerNode(peerNode);

                } else if ((request.msgType == MSG_TYPE_SAYONARA)
                        && (this->cds.name.Compare(request.senderBody.name))) {
                    /* Got an explicit disconnect. */
                    TRACE(Trace::LEVEL_INFO, "Response thread received "
                        "MSG_TYPE_SAYONARA from %s.\n",
                        peerAddr.ToStringA().PeekBuffer());

                    peerNode.address = request.senderBody.sockAddr;
                    peerNode.discoveryAddr = SocketAddress(peerAddr, 
                        this->cds.bindAddr.GetPort());
                    this->cds.removePeerNode(peerNode);

                } else if (request.msgType >= MSG_TYPE_USER) {
                    /* Received user message. */
                    this->cds.fireUserMessage(peerAddr, request.msgType, 
                        request.userData);
                }
            } /* end if (response.magicNumber == MAGIC_NUMBER) */

        } catch (SocketException e) {
            TRACE(Trace::LEVEL_WARN, "A socket error occurred in the "
                "discovery receiver thread. The error code is %d (\"%s\").\n",
                e.GetErrorCode(), e.GetMsgA());
        } catch (...) {
            TRACE(Trace::LEVEL_ERROR, "The discovery receiver caught an "
                "unexpected exception.\n");
            return 2;
        }

    } /* end while (this->isRunning) */

    try {
        Socket::Cleanup();
    } catch (SocketException e) {
        TRACE(Trace::LEVEL_ERROR, "Socket cleanup failed in the discovery "
            "receiver thread. The error code is %d (\"%s\").\n", 
            e.GetErrorCode(), e.GetMsgA());
    }

    return 0;
}


/*
 * vislib::net::ClusterDiscoveryService::Receiver::Terminate
 */
bool vislib::net::ClusterDiscoveryService::Receiver::Terminate(void) {
    // TODO: Should perhaps be protected by crit sect.
    this->isRunning = false;
    return true;
}

// End of nested class Receiver
////////////////////////////////////////////////////////////////////////////////


/*
 * vislib::net::ClusterDiscoveryService::addPeerNode
 */
void vislib::net::ClusterDiscoveryService::addPeerNode(const PeerNode& node) {
    this->critSect.Lock();

    PeerNode *localNode = this->peerNodes.Find(node);

    if (localNode != NULL) {
        /* Already known, reset disconnect chance. */
        localNode->cntResponseChances = this->cntResponseChances + 1;

    } else {
        /* Not known, so add it and fire event. */
        this->peerNodes.Append(node);

        ListenerList::Iterator it = const_cast<ClusterDiscoveryService *>(
            this)->listeners.GetIterator();
        while (it.HasNext()) {
            it.Next()->OnNodeFound(*this, node.address);
        }
    }

    this->critSect.Unlock();
}


/*
 * vislib::net::ClusterDiscoveryService::fireUserMessage
 */
void vislib::net::ClusterDiscoveryService::fireUserMessage(
        const SocketAddress& sender, const UINT16 msgType, 
        const BYTE *msgBody) const {
    INT_PTR idx = 0;        // Index of sender PeerNode.

    this->critSect.Lock();
    
    if ((idx = this->peerFromDiscoveryAddr(sender.GetIPAddress())) >= 0) {
   
        // TODO: We need a const iterator.
        ListenerList::Iterator it = const_cast<ClusterDiscoveryService *>(
            this)->listeners.GetIterator();
        while (it.HasNext()) {
            // TODO: Should avoid cast.
            it.Next()->OnUserMessage(*this, 
                this->peerNodes[static_cast<INT>(idx)].address, msgType, 
                msgBody);
        }
    }

    this->critSect.Unlock();
}


/*
 * vislib::net::ClusterDiscoveryService::peerFromDiscoveryAddr
 */
INT_PTR vislib::net::ClusterDiscoveryService::peerFromDiscoveryAddr(
        const IPAddress& addr) const {
    // TODO: Think of faster solution.  
    INT_PTR retval = -1;

    //this->critSect.Lock();
    for (SIZE_T i = 0; i < this->peerNodes.Count(); i++) {
        if (this->peerNodes[i].discoveryAddr.GetIPAddress() == addr) {
            retval = i;
            break;
        }
    }
    //this->critSect.Unlock();

    return retval;
}


/*
 * vislib::net::ClusterDiscoveryService::peerFromResponseAddr
 */
INT_PTR vislib::net::ClusterDiscoveryService::peerFromResponseAddr(
        const SocketAddress& addr) const {
    // TODO: Think of faster solution.
    INT_PTR retval = -1;

    //this->critSect.Lock();
    for (SIZE_T i = 0; i < this->peerNodes.Count(); i++) {
        if (this->peerNodes[i].address == addr) {
            retval = i;
            break;
        }
    }
    //this->critSect.Unlock();

    return retval;
}


/*
 * vislib::net::ClusterDiscoveryService::prepareRequest
 */
void vislib::net::ClusterDiscoveryService::prepareRequest(void) {
    this->critSect.Lock();

    for (SIZE_T i = 0; i < this->peerNodes.Count(); i++) {
        if (this->peerNodes[i].cntResponseChances == 0) {
            
            /* Fire event. */
            // TODO: We need a const iterator.
            ListenerList::Iterator it = const_cast<ClusterDiscoveryService *>(
                this)->listeners.GetIterator();
            while (it.HasNext()) {
                it.Next()->OnNodeLost(*this, this->peerNodes[i].address,
                    ClusterDiscoveryListener::LOST_IMLICITLY);
            }

            /* Remove peer from list. */
            this->peerNodes.Erase(i);
            i--;

        } else {
            /* Decrement response chance for upcoming request. */
            this->peerNodes[i].cntResponseChances--;
        }
    }

    this->critSect.Unlock();
}

/*
 * vislib::net::ClusterDiscoveryService::prepareUserMessage
 */
void vislib::net::ClusterDiscoveryService::prepareUserMessage(
        Message& outMsg, const UINT16 msgType, const void *msgBody, 
        const SIZE_T msgSize) {

    /* Check parameters. */
    if (msgType < MSG_TYPE_USER) {
        throw IllegalParamException("msgType", __FILE__, __LINE__);
    }
    if (msgBody == NULL) {
        throw IllegalParamException("msgBody", __FILE__, __LINE__);
    }
    if (msgSize > MAX_USER_DATA) {
        throw IllegalParamException("msgSize", __FILE__, __LINE__);
    }

    // Assert some stuff.
    ASSERT(sizeof(outMsg) == MAX_USER_DATA + 4);
    ASSERT(reinterpret_cast<BYTE *>(&(outMsg.userData)) 
       == reinterpret_cast<BYTE *>(&outMsg) + 4);
    ASSERT(msgType >= MSG_TYPE_USER);
    ASSERT(msgBody != NULL);
    ASSERT(msgSize <= MAX_USER_DATA);

    /* Prepare the message. */
    outMsg.magicNumber = MAGIC_NUMBER;
    outMsg.msgType = msgType;
    ::ZeroMemory(outMsg.userData, MAX_USER_DATA);
    ::memcpy(outMsg.userData, msgBody, msgSize);

    /* Lazy creation of our socket. */
    if (!this->userMsgSocket.IsValid()) {
        this->userMsgSocket.Create(Socket::FAMILY_INET, Socket::TYPE_DGRAM,
            Socket::PROTOCOL_UDP);
    }
}


/*
 * vislib::net::ClusterDiscoveryService::removePeerNode
 */
void vislib::net::ClusterDiscoveryService::removePeerNode(
        const PeerNode& node) {
    INT_PTR idx = 0;
    
    this->critSect.Lock();

    if ((idx = this->peerNodes.IndexOf(node)) >= 0) {

        /* Fire event. */
        // TODO: We need a const iterator.
        ListenerList::Iterator it = const_cast<ClusterDiscoveryService *>(
            this)->listeners.GetIterator();
        while (it.HasNext()) {
            it.Next()->OnNodeLost(*this, SocketAddress(node.address),
                ClusterDiscoveryListener::LOST_EXPLICITLY);
        }

        /* Remove peer from list. */
        this->peerNodes.Erase(idx);
    }

    this->critSect.Unlock();
}


/*
 * vislib::net::ClusterDiscoveryService::MAGIC_NUMBER
 */
const UINT16 vislib::net::ClusterDiscoveryService::MAGIC_NUMBER 
    = static_cast<UINT16>('v') << 8 | static_cast<UINT16>('l');


/*
 * vislib::net::ClusterDiscoveryService::MSG_TYPE_IAMHERE
 */
const UINT16 vislib::net::ClusterDiscoveryService::MSG_TYPE_IAMHERE = 1;


/*
 * vislib::net::ClusterDiscoveryService::MSG_TYPE_SAYONARA
 */
const UINT16 vislib::net::ClusterDiscoveryService::MSG_TYPE_SAYONARA = 3;
