/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
 * Copyright (c) 2014-2022,  Regents of the University of California,
 *                           Arizona Board of Regents,
 *                           Colorado State University,
 *                           University Pierre & Marie Curie, Sorbonne University,
 *                           Washington University in St. Louis,
 *                           Beijing Institute of Technology,
 *                           The University of Memphis.
 *
 * This file is part of NFD (Named Data Networking Forwarding Daemon).
 * See AUTHORS.md for complete list of NFD authors and contributors.
 *
 * NFD is free software: you can redistribute it and/or modify it under the terms
 * of the GNU General Public License as published by the Free Software Foundation,
 * either version 3 of the License, or (at your option) any later version.
 *
 * NFD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
 * without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
 * PURPOSE.  See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * NFD, e.g., in COPYING.md file.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef NFD_TESTS_DAEMON_FACE_UNICAST_UDP_TRANSPORT_FIXTURE_HPP
#define NFD_TESTS_DAEMON_FACE_UNICAST_UDP_TRANSPORT_FIXTURE_HPP

#include "face/unicast-udp-transport.hpp"
#include "face/face.hpp"

#include "tests/test-common.hpp"
#include "tests/daemon/limited-io.hpp"
#include "tests/daemon/face/dummy-link-service.hpp"

namespace nfd::tests {

namespace ip = boost::asio::ip;
using ip::udp;
using face::UnicastUdpTransport;

class UnicastUdpTransportFixture : public GlobalIoFixture
{
protected:
  void
  initialize(ip::address address,
             ndn::nfd::FacePersistency persistency = ndn::nfd::FACE_PERSISTENCY_PERSISTENT)
  {
    udp::socket sock(g_io);
    sock.connect(udp::endpoint(address, 7070));
    localEp = sock.local_endpoint();

    remoteConnect(address);

    face = make_unique<Face>(make_unique<DummyLinkService>(),
                             make_unique<UnicastUdpTransport>(std::move(sock), persistency, 3_s));
    transport = static_cast<UnicastUdpTransport*>(face->getTransport());
    receivedPackets = &static_cast<DummyLinkService*>(face->getLinkService())->receivedPackets;

    BOOST_REQUIRE_EQUAL(transport->getState(), face::TransportState::UP);
  }

  void
  remoteConnect(ip::address address = ip::address_v4::loopback())
  {
    udp::endpoint remoteEp(address, 7070);
    remoteSocket.open(remoteEp.protocol());
    remoteSocket.set_option(udp::socket::reuse_address(true));
    remoteSocket.bind(remoteEp);
    remoteSocket.connect(localEp);
  }

  void
  remoteRead(std::vector<uint8_t>& buf, bool needToCheck = true)
  {
    remoteSocket.async_receive(boost::asio::buffer(buf),
      [this, needToCheck] (const boost::system::error_code& error, size_t) {
        if (needToCheck) {
          BOOST_REQUIRE_EQUAL(error, boost::system::errc::success);
        }
        limitedIo.afterOp();
      });
    BOOST_REQUIRE_EQUAL(limitedIo.run(1, 1_s), LimitedIo::EXCEED_OPS);
  }

  void
  remoteWrite(const std::vector<uint8_t>& buf, bool needToCheck = true)
  {
    remoteSocket.async_send(boost::asio::buffer(buf),
      [needToCheck] (const auto& error, size_t) {
        if (needToCheck) {
          BOOST_REQUIRE_EQUAL(error, boost::system::errc::success);
        }
      });
    limitedIo.defer(1_s);
  }

protected:
  LimitedIo limitedIo;
  UnicastUdpTransport* transport = nullptr;
  udp::endpoint localEp;
  udp::socket remoteSocket{g_io};
  std::vector<RxPacket>* receivedPackets = nullptr;

private:
  unique_ptr<Face> face;
};

} // namespace nfd::tests

#endif // NFD_TESTS_DAEMON_FACE_UNICAST_UDP_TRANSPORT_FIXTURE_HPP
