/*
 * xp_gauge_streamer - avionics display streamer for X-Plane 12
 * Copyright (c) 2026 thWelly
 *
 * Licensed under the MIT License. See the LICENSE file in the
 * project root for full license information.
 */

#include "network_info.hpp"

#include <arpa/inet.h>
#include <ifaddrs.h>
#include <net/if.h>
#include <netinet/in.h>
#include <sys/socket.h>

#include <memory>

namespace xp_gauge_streamer
{

namespace
{

bool carries_lan_traffic(const ifaddrs &interface)
{
    if (interface.ifa_addr == nullptr || interface.ifa_addr->sa_family != AF_INET)
        return false;

    const unsigned flags = interface.ifa_flags;
    return (flags & IFF_UP) != 0 && (flags & IFF_LOOPBACK) == 0 && (flags & IFF_POINTOPOINT) == 0;
}

std::string address_of(const ifaddrs &interface)
{
    const auto *address               = reinterpret_cast<const sockaddr_in *>(interface.ifa_addr);
    char        text[INET_ADDRSTRLEN] = {};

    if (inet_ntop(AF_INET, &address->sin_addr, text, sizeof(text)) == nullptr)
        return {};

    return text;
}

} // namespace

std::string local_network_address()
{
    ifaddrs *raw = nullptr;
    if (getifaddrs(&raw) != 0)
        return {};

    const std::unique_ptr<ifaddrs, decltype(&freeifaddrs)> interfaces(raw, &freeifaddrs);

    for (const ifaddrs *interface = interfaces.get(); interface != nullptr; interface = interface->ifa_next)
    {
        if (carries_lan_traffic(*interface))
            return address_of(*interface);
    }

    return {};
}

} // namespace xp_gauge_streamer
