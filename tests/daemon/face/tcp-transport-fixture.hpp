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

#ifndef NFD_TESTS_DAEMON_FACE_TCP_TRANSPORT_FIXTURE_HPP
#define NFD_TESTS_DAEMON_FACE_TCP_TRANSPORT_FIXTURE_HPP

#include "face/tcp-transport.hpp"
#include "face/face.hpp"

#include "tests/test-common.hpp"
#include "tests/daemon/limited-io.hpp"
#include "tests/daemon/face/dummy-link-service.hpp"

namespace nfd::tests {

namespace ip = boost::asio::ip;
using ip::tcp;
using face::TcpTransport;

class TcpTransportFixture : public GlobalIoFixture
{
protected:
  void
  startAccept(const tcp::endpoint& remoteEp)
  {
    BOOST_REQUIRE(!acceptor.is_open());
    acceptor.open(remoteEp.protocol());
    acceptor.set_option(tcp::acceptor::reuse_address(true));
    acceptor.bind(remoteEp);
    acceptor.listen(1);
    acceptor.async_accept(remoteSocket, [this] (const boost::system::error_code& error) {
      BOOST_REQUIRE_EQUAL(error, boost::system::errc::success);
      limitedIo.afterOp();
    });
  }

  void
  stopAccept()
  {
    BOOST_REQUIRE(acceptor.is_open());
    acceptor.close();
  }

  void
  initialize(ip::address address,
             ndn::nfd::FacePersistency persistency = ndn::nfd::FACE_PERSISTENCY_PERSISTENT)
  {
    tcp::endpoint remoteEp(address, 7070);
    startAccept(remoteEp);

    tcp::socket sock(g_io);
    sock.async_connect(remoteEp, [this] (const boost::system::error_code& error) {
      BOOST_REQUIRE_EQUAL(error, boost::system::errc::success);
      limitedIo.afterOp();
    });

    BOOST_REQUIRE_EQUAL(limitedIo.run(2, 1_s), LimitedIo::EXCEED_OPS);

    localEp = sock.local_endpoint();

    ndn::nfd::FaceScope scope;
    if (sock.local_endpoint().address().is_loopback() && sock.remote_endpoint().address().is_loopback()) {
      scope = ndn::nfd::FACE_SCOPE_LOCAL;
    }
    else {
      scope = ndn::nfd::FACE_SCOPE_NON_LOCAL;
    }

    face = make_unique<Face>(make_unique<DummyLinkService>(),
                             make_unique<TcpTransport>(std::move(sock), persistency, scope));
    transport = static_cast<TcpTransport*>(face->getTransport());
    receivedPackets = &static_cast<DummyLinkService*>(face->getLinkService())->receivedPackets;

    BOOST_REQUIRE_EQUAL(transport->getState(), face::TransportState::UP);
  }

  void
  remoteWrite(const std::vector<uint8_t>& buf, bool needToCheck = true)
  {
    boost::asio::async_write(remoteSocket, boost::asio::buffer(buf),
      [needToCheck] (const auto& error, size_t) {
        if (needToCheck) {
          BOOST_REQUIRE_EQUAL(error, boost::system::errc::success);
        }
      });
    limitedIo.defer(1_s);
  }

protected:
  LimitedIo limitedIo;
  TcpTransport* transport = nullptr;
  tcp::endpoint localEp;
  tcp::socket remoteSocket{g_io};
  std::vector<RxPacket>* receivedPackets = nullptr;

private:
  tcp::acceptor acceptor{g_io};
  unique_ptr<Face> face;
};

} // namespace nfd::tests

#endif // NFD_TESTS_DAEMON_FACE_TCP_TRANSPORT_FIXTURE_HPP
