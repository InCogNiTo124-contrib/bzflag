/* bzflag
 * Copyright (c) 1993-2025 Tim Riker
 *
 * This package is free software;  you can redistribute it and/or
 * modify it under the terms of the license found in the file
 * named COPYING that should have accompanied this file.
 *
 * THIS PACKAGE IS PROVIDED ``AS IS'' AND WITHOUT ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, WITHOUT LIMITATION, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE.
 */

/* interface header */
#include "AresHandler.h"

/* system implementation headers */
#include <cerrno>
#include <cstring>
#include <mutex>

bool AresHandler::globallyInited = false;
std::mutex callback_mutex;

AresHandler::AresHandler(int _index)
    : index(_index), status(None)
{
    // clear the host address
    memset(&hostAddress, 0, sizeof(hostAddress));

    /* ask for local "hosts" lookups too */
    static char lookups[] = "fb";
    struct ares_options opts;
    memset(&opts, 0, sizeof(opts));
    int    flags = 0;

    opts.lookups = lookups;
    flags       |= ARES_OPT_LOOKUPS;
    opts.evsys   = ARES_EVSYS_DEFAULT;
    flags       |= ARES_OPT_EVENT_THREAD;

    /* start up our resolver */
    if (!globallyInited)
    {
        aresFailed = !globalInit();
        if (aresFailed)
        {
            status = Failed;
            logDebugMessage(2,"Ares failed initializing library\n");
        }
    }

    /* start up our resolver */
    int code = ares_init_options (&aresChannel, &opts, flags);
    aresFailed = (code != ARES_SUCCESS);
    if (aresFailed)
    {
        status = Failed;
        logDebugMessage(2,"Ares Failed initializing\n");
    }
}

AresHandler::~AresHandler()
{
    if (aresFailed)
        return;
    ares_destroy(aresChannel);
}

bool AresHandler::globalInit()
{
    if (!globallyInited)
    {
        if (ares_library_init(ARES_LIB_INIT_ALL) == ARES_SUCCESS)
            globallyInited = true;
    }
    return globallyInited;
}

void AresHandler::globalShutdown()
{
    if (globallyInited)
        ares_library_cleanup();
}

void AresHandler::queryHostname(const struct sockaddr *clientAddr)
{
    if (aresFailed)
        return;

    status = HbAPending;
    // launch the asynchronous query to look up this hostname
    ares_getnameinfo(aresChannel, clientAddr, sizeof(sockaddr_in),
                     ARES_NI_NAMEREQD | ARES_NI_LOOKUPHOST, staticCallbackNameInfo, (void *)this);
    logDebugMessage(2,"Player [%d] submitted reverse resolve query\n", index);
}

void AresHandler::queryHost(const char *name)
{
    if (aresFailed)
        return;
    ares_cancel(aresChannel);

    if (!name)
    {
        status = Failed;
        return;
    }

    if (inet_aton(name, &hostAddress) != 0)
    {
        status = HbNSucceeded;
        return;
    }

    // launch the asynchronous query to look up this hostname
    status = HbNPending;

    struct ares_addrinfo_hints hints;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;

    ares_getaddrinfo(aresChannel, name, NULL, &hints, staticCallbackAddrInfo,
                     (void *)this);
}

void AresHandler::staticCallbackNameInfo(void *arg, int status,
        int, char *node, char *service)
{
    if (status != ARES_EDESTRUCTION)
        ((AresHandler *)arg)->callbackNameInfo(status, node, service);
}

void AresHandler::callbackNameInfo(int callbackStatus, char *node, char *)
{
    const std::lock_guard<std::mutex> lock(callback_mutex);

    if (callbackStatus != ARES_SUCCESS || !node)
    {
        logDebugMessage(1,"Player [%d] failed to resolve: error %d\n", index,
                        callbackStatus);
        status = Failed;
    }
    else if (status == HbAPending)
    {
        hostName = node;
        status = HbASucceeded;
        logDebugMessage(2,"Player [%d] resolved to %s\n", index, hostName.c_str());
    }
}

void AresHandler::staticCallbackAddrInfo(void *arg, int status,
        int, struct ares_addrinfo *result)
{
    if (status != ARES_EDESTRUCTION)
        ((AresHandler *)arg)->callbackAddrInfo(status, result);
    ares_freeaddrinfo(result);
}

void AresHandler::callbackAddrInfo(int callbackStatus, struct ares_addrinfo *result)
{
    const std::lock_guard<std::mutex> lock(callback_mutex);

    if (callbackStatus != ARES_SUCCESS || !result)
    {
        logDebugMessage(1,"Player [%d] failed to resolve: error %d\n", index,
                        callbackStatus);
        status = Failed;
    }

    if (status == HbNPending)
    {
        auto nodes = result->nodes;
        if (nodes != nullptr)
        {
            struct sockaddr_in *ipv4 = (struct sockaddr_in *)nodes->ai_addr;

            if (ipv4)
            {
                memcpy(&hostAddress, &ipv4->sin_addr, sizeof(hostAddress));
                status = HbNSucceeded;
            }
            else
                status = Failed;
        }
        else
            status = Failed;
    }
}

const char *AresHandler::getHostname()
{
    if (!callback_mutex.try_lock())
        return "";
    const char *host = hostName.c_str();
    callback_mutex.unlock();
    return host;
}

AresHandler::ResolutionStatus AresHandler::getHostAddress(struct in_addr
        *clientAddr)
{
    if (!callback_mutex.try_lock())
        return HbNPending;

    const ResolutionStatus oldStatus = status;
    if (status == HbNSucceeded)
        memcpy(clientAddr, &hostAddress, sizeof(hostAddress));
    callback_mutex.unlock();

    return oldStatus;
}

// Local Variables: ***
// mode: C++ ***
// tab-width: 4 ***
// c-basic-offset: 4 ***
// indent-tabs-mode: nil ***
// End: ***
// ex: shiftwidth=4 tabstop=4
