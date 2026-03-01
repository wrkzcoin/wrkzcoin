// Copyright (c) 2012-2017, The CryptoNote developers, The Bytecoin developers
// Copyright (c) 2018-2019, The TurtleCoin Developers
// Copyright (c) 2018-2026, The WrkzCoin developers
//
// Please see the included LICENSE file for more information.

#include "ContextGroup.h"

#include <cassert>

namespace System
{
    ContextGroup::ContextGroup(Dispatcher &dispatcher): dispatcher(&dispatcher)
    {
        contextGroup.firstContext = nullptr;
    }

    ContextGroup::ContextGroup(ContextGroup &&other): dispatcher(other.dispatcher)
    {
        if (dispatcher != nullptr)
        {
            assert(other.contextGroup.firstContext == nullptr);
            contextGroup.firstContext = nullptr;
            other.dispatcher = nullptr;
        }
    }

    ContextGroup::~ContextGroup()
    {
        if (dispatcher != nullptr)
        {
            interrupt();
            wait();
        }
    }

    ContextGroup &ContextGroup::operator=(ContextGroup &&other)
    {
        assert(dispatcher == nullptr || contextGroup.firstContext == nullptr);
        dispatcher = other.dispatcher;
        if (dispatcher != nullptr)
        {
            assert(other.contextGroup.firstContext == nullptr);
            contextGroup.firstContext = nullptr;
            other.dispatcher = nullptr;
        }

        return *this;
    }

    void ContextGroup::interrupt()
    {
        assert(dispatcher != nullptr);
        for (NativeContext *context = contextGroup.firstContext; context != nullptr; context = context->groupNext)
        {
            dispatcher->interrupt(context);
        }
    }

    void ContextGroup::spawn(std::function<void()> &&procedure)
    {
        assert(dispatcher != nullptr);
        NativeContext &context = dispatcher->getReusableContext();
        if (contextGroup.firstContext != nullptr)
        {
            // Fast path: lastContext is a live member of this group and is the tail.
            // Slow path: lastContext has been recycled back to the pool (group == nullptr
            // after contextProcedure clears it) or its groupNext is unexpectedly non-null.
            // In the slow path we scan from firstContext — which only traverses confirmed
            // group members — so we never write groupNext on a recycled context and cannot
            // create a ghost link that later breaks the unlink assertions in contextProcedure.
            if (contextGroup.lastContext->group != &contextGroup
                || contextGroup.lastContext->groupNext != nullptr)
            {
                NativeContext *tail = contextGroup.firstContext;
                while (tail->groupNext != nullptr)
                {
                    tail = tail->groupNext;
                }
                contextGroup.lastContext = tail;
            }
            context.groupPrev = contextGroup.lastContext;
            contextGroup.lastContext->groupNext = &context;
        }
        else
        {
            context.groupPrev = nullptr;
            contextGroup.firstContext = &context;
            contextGroup.firstWaiter = nullptr;
        }

        context.interrupted = false;
        context.group = &contextGroup;
        context.groupNext = nullptr;
        context.procedure = std::move(procedure);
        contextGroup.lastContext = &context;
        dispatcher->pushContext(&context);
    }

    void ContextGroup::wait()
    {
        if (contextGroup.firstContext != nullptr)
        {
            NativeContext *context = dispatcher->getCurrentContext();
            context->next = nullptr;

            assert(!context->inExecutionQueue);
            context->inExecutionQueue = true;

            if (contextGroup.firstWaiter != nullptr)
            {
                assert(contextGroup.lastWaiter->next == nullptr);
                contextGroup.lastWaiter->next = context;
            }
            else
            {
                contextGroup.firstWaiter = context;
            }

            contextGroup.lastWaiter = context;
            dispatcher->dispatch();
            assert(context == dispatcher->getCurrentContext());
        }
    }

} // namespace System
