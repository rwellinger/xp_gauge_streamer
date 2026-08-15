/*
 * xp_gauge_streamer - avionics display streamer for X-Plane 12
 * Copyright (c) 2026 thWelly
 *
 * Licensed under the MIT License. See the LICENSE file in the
 * project root for full license information.
 */

/*
 * The Windows half of local_network_address(). POSIX' getifaddrs() does not
 * exist here; GetAdaptersAddresses() is the equivalent, with the usual
 * grow-the-buffer-and-retry dance. Deliberately shaped like the POSIX file next
 * to it — same two helpers, same order — so the pair reads as one idea.
 */

#include "network_info.hpp"

#include <winsock2.h>
// clang-format off
#include <ws2tcpip.h>
#include <iphlpapi.h>
// clang-format on

#include <vector>

namespace xp_gauge_streamer
{

namespace
{

constexpr unsigned long ADAPTER_FLAGS = GAA_FLAG_SKIP_ANYCAST | GAA_FLAG_SKIP_MULTICAST | GAA_FLAG_SKIP_DNS_SERVER;

// Mirrors the POSIX predicate: up, not loopback, not point-to-point. A VPN or a
// virtual switch passes this too — see the note in local_network_address().
bool carries_lan_traffic(const IP_ADAPTER_ADDRESSES &adapter)
{
    return adapter.OperStatus == IfOperStatusUp && adapter.IfType != IF_TYPE_SOFTWARE_LOOPBACK &&
           adapter.IfType != IF_TYPE_PPP;
}

std::string address_of(const IP_ADAPTER_ADDRESSES &adapter)
{
    for (const IP_ADAPTER_UNICAST_ADDRESS *entry = adapter.FirstUnicastAddress; entry != nullptr; entry = entry->Next)
    {
        if (entry->Address.lpSockaddr == nullptr || entry->Address.lpSockaddr->sa_family != AF_INET)
            continue;

        const auto *address               = reinterpret_cast<const sockaddr_in *>(entry->Address.lpSockaddr);
        char        text[INET_ADDRSTRLEN] = {};

        if (inet_ntop(AF_INET, &address->sin_addr, text, sizeof(text)) == nullptr)
            continue;

        return text;
    }

    return {};
}

// GetAdaptersAddresses reports the size it wants when the buffer is too small.
// The documented advice is to start at 15 KB and retry; the loop is bounded
// because the answer can grow between the two calls.
std::vector<unsigned char> adapter_table()
{
    constexpr int              MAX_ATTEMPTS = 3;
    std::vector<unsigned char> buffer(15 * 1024);

    for (int attempt = 0; attempt < MAX_ATTEMPTS; ++attempt)
    {
        auto        size   = static_cast<ULONG>(buffer.size());
        const ULONG result = GetAdaptersAddresses(AF_INET, ADAPTER_FLAGS, nullptr,
                                                  reinterpret_cast<IP_ADAPTER_ADDRESSES *>(buffer.data()), &size);

        if (result == NO_ERROR)
            return buffer;

        if (result != ERROR_BUFFER_OVERFLOW)
            return {};

        buffer.resize(size);
    }

    return {};
}

} // namespace

// With more than one candidate — a VPN, Hyper-V's vEthernet, WSL, a docker
// bridge — the first one wins, exactly as in the POSIX implementation. Windows
// makes that visible more often, because the list arrives in interface-metric
// order, which ranks by routing cost rather than by which address a tablet on
// the sofa can reach. The address is shown, never bound to, so a wrong guess
// costs the user a manual correction in the browser rather than a broken server.
std::string local_network_address()
{
    std::vector<unsigned char> buffer = adapter_table();
    if (buffer.empty())
        return {};

    const auto *first = reinterpret_cast<const IP_ADAPTER_ADDRESSES *>(buffer.data());

    for (const IP_ADAPTER_ADDRESSES *adapter = first; adapter != nullptr; adapter = adapter->Next)
    {
        if (!carries_lan_traffic(*adapter))
            continue;

        std::string address = address_of(*adapter);
        if (!address.empty())
            return address;
    }

    return {};
}

} // namespace xp_gauge_streamer
