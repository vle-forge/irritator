// Copyright (c) 2020 INRA Distributed under the Boost Software License,
// Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#include <irritator/core.hpp>

namespace irt {

template<typename Time>
struct model_generator
{
    output_port_id y[1];
    Time sigma;

    status initialize(simulation& /*sim*/)
    {
        sigma = 1;

        return status::success;
    }

    status lambda(simulation& sim)
    {
        if (auto* port = sim.output_ports.try_to_get(y[0]); port)
            port->messages.emplace_front(0.0);

        return status::success;
    }
};

} // namespace irt
