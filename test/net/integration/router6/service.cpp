// This file is a part of the IncludeOS unikernel - www.includeos.org
//
// Copyright 2015 Oslo and Akershus University College of Applied Sciences
// and Alfred Bratterud
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include <service>
#include <net/inet>
#include <net/router.hpp>
using namespace net;

static std::unique_ptr<Router<IP6>> router;

static bool
route_checker(IP6::addr addr)
{
  INFO("Route checker", "asked for route to IP %s", addr.to_string().c_str());

  bool have_route = router->route_check(addr);

  INFO("Route checker", "The router says %i", have_route);

  if (have_route)
    INFO2("* Responding YES");
  else
    INFO2("* Responding NO");

  return have_route;
}

static void
ip_forward (IP6::IP_packet_ptr pckt, Inet& stack, Conntrack<IP6>::Entry_ptr)
{
  Inet* route = router->get_first_interface(pckt->ip_dst());

  if (not route){
    INFO("ip_fwd", "No route found for %s dropping\n", pckt->ip_dst().to_string().c_str());
    return;
  }

  if (route == &stack) {
    INFO("ip_fwd", "* Oh, this packet was for me, sow why was it forwarded here? \n");
    return;
  }

  debug("[ ip_fwd ] %s transmitting packet to %s",stack.ifname().c_str(), route->ifname().c_str());
  route->ip_obj().ship(std::move(pckt));
}


void Service::start(const std::string&)
{
  auto& inet = Inet::stack<0>();
  inet.network_config6({ 0xfe80, 0, 0, 0, 0xe823, 0xfcff, 0xfef4, 0x85cd },    // IP
                      112,                                                      // Prefix6
                      { 0xfe80,  0,  0, 0, 0xe823, 0xfcff, 0xfef4, 0x83e7 });  // Gateway

  INFO("Router","Interface 1 IP: %s\n", inet.ip6_addr().str().c_str());

  auto& inet2 = Inet::stack<1>();
  inet2.network_config6({ 0xfe80, 0, 0, 0, 0xabcd, 0xabcd, 0x1234, 0x5678 },  // IP
                      112,                                                   // Prefix6
                      { 0xfe80, 0, 0, 0, 0xabcd, 0xabcd, 0x1234, 0x8367}); // Gateway

  INFO("Router","Interface2 IP: %s\n", inet2.ip6_addr().str().c_str());


  // IP Forwarding
  inet.ip6_obj().set_packet_forwarding(ip_forward);
  inet2.ip6_obj().set_packet_forwarding(ip_forward);

  // ARP Route checker
  inet.set_route_checker6(route_checker);
  inet2.set_route_checker6(route_checker);


  /** Some times it's nice to add dest. to arp-cache to avoid having it respond to arp */
  // inet2.cache_link_ip({10,42,42,2}, {0x10,0x11, 0x12, 0x13, 0x14, 0x15});
  // inet2.cache_link_ip({10,42,42,2}, {0x1e,0x5f,0x30,0x98,0x19,0x8b});
  // inet2.cache_link_ip({10,42,42,2}, {0xc0,0x00, 0x10, 0x00, 0x00, 0x02});

  // Routing table
  Router<IP6>::Routing_table routing_table{
    {{0xfe80, 0, 0, 0, 0xabcd, 0xabcd, 0x1234, 0 }, 112,
    {0xfe80, 0, 0, 0, 0xabcd, 0xabcd, 0x1234, 0x8367}, inet2 , 1 },
    {{ 0xfe80,  0,  0, 0, 0xe823, 0xfcff, 0xfef4, 0}, 112,
    {0xfe80,  0,  0, 0, 0xe823, 0xfcff, 0xfef4, 0x83e7}, inet , 1 }
  };

  router = std::make_unique<Router<IP6>>(routing_table);

  INFO("Router", "Routing enabled - routing table:");

  for (auto r : routing_table)
    INFO2("* %s/%i -> %s / %s, cost %i", r.net().str().c_str(),
          __builtin_popcount(r.netmask()),
          r.interface()->ifname().c_str(),
          r.nexthop().to_string().c_str(),
          r.cost());
  printf("\n");
  INFO("Router","Service ready");
}
