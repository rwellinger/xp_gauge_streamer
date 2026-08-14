/*
 * xp_gauge_streamer - GNS430/530 display streamer for X-Plane 12
 * Copyright (c) 2026 thWelly
 *
 * Licensed under the MIT License. See the LICENSE file in the
 * project root for full license information.
 */

#pragma once

#include <string>

namespace xp_gauge_streamer
{

// The machine's address in the local network, e.g. "10.0.1.134". This is what
// belongs on the tablet — the server's bind address is 0.0.0.0, which is no use
// to anyone typing it in. Empty when the machine is not on a network.
std::string local_network_address();

} // namespace xp_gauge_streamer
