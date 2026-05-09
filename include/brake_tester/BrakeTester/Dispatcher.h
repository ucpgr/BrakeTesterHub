//
// Created by didal on 06/09/2025.
//

#ifndef BRAKETESTERRESULTMEMORYWRITER_DISPATCHER_H
#define BRAKETESTERRESULTMEMORYWRITER_DISPATCHER_H

#include <functional>
#include <stdexcept>
#include <tuple>
#include <utility>
#include <variant>

#include "Frame.h"

namespace BrakeTester
{
    class Dispatcher
    {
        FrameHandlers m_Handlers{};

    public:
        Dispatcher() = default;
        explicit Dispatcher(FrameHandlers handlers) : m_Handlers(std::move(handlers)) {}

        void operator()(const FrameVariant &frame) const
        {
            std::visit(m_Handlers, frame);
        }
    };
} // BrakeTester

#endif //BRAKETESTERRESULTMEMORYWRITER_DISPATCHER_H