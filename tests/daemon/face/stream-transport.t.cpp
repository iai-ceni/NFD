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

#include "tcp-transport-fixture.hpp"
#include "unix-stream-transport-fixture.hpp"

#include "transport-test-common.hpp"

#include <boost/asio/read.hpp>
#include <boost/mpl/vector.hpp>

namespace nfd::tests {

using namespace nfd::face;

BOOST_AUTO_TEST_SUITE(Face)
BOOST_AUTO_TEST_SUITE(TestStreamTransport)

using StreamTransportFixtures = boost::mpl::vector<
  GENERATE_IP_TRANSPORT_FIXTURE_INSTANTIATIONS(TcpTransportFixture),
  UnixStreamTransportFixture
>;

BOOST_FIXTURE_TEST_CASE_TEMPLATE(Send, T, StreamTransportFixtures, T)
{
  TRANSPORT_TEST_INIT();

  auto block1 = ndn::encoding::makeStringBlock(300, "hello");
  this->transport->send(block1);
  BOOST_CHECK_EQUAL(this->transport->getCounters().nOutPackets, 1);
  BOOST_CHECK_EQUAL(this->transport->getCounters().nOutBytes, block1.size());

  auto block2 = ndn::encoding::makeStringBlock(301, "world");
  this->transport->send(block2);
  BOOST_CHECK_EQUAL(this->transport->getCounters().nOutPackets, 2);
  BOOST_CHECK_EQUAL(this->transport->getCounters().nOutBytes, block1.size() + block2.size());

  std::vector<uint8_t> readBuf(block1.size() + block2.size());
  boost::asio::async_read(this->remoteSocket, boost::asio::buffer(readBuf),
    [this] (const boost::system::error_code& error, size_t) {
      BOOST_REQUIRE_EQUAL(error, boost::system::errc::success);
      this->limitedIo.afterOp();
    });

  BOOST_REQUIRE_EQUAL(this->limitedIo.run(1, 1_s), LimitedIo::EXCEED_OPS);

  BOOST_CHECK_EQUAL_COLLECTIONS(readBuf.begin(), readBuf.begin() + block1.size(), block1.begin(), block1.end());
  BOOST_CHECK_EQUAL_COLLECTIONS(readBuf.begin() + block1.size(), readBuf.end(),   block2.begin(), block2.end());
  BOOST_CHECK_EQUAL(this->transport->getState(), TransportState::UP);
}

BOOST_FIXTURE_TEST_CASE_TEMPLATE(ReceiveNormal, T, StreamTransportFixtures, T)
{
  TRANSPORT_TEST_INIT();

  auto pkt1 = ndn::encoding::makeStringBlock(300, "hello");
  ndn::Buffer buf1(pkt1.begin(), pkt1.end());
  this->remoteWrite(buf1);

  BOOST_CHECK_EQUAL(this->transport->getCounters().nInPackets, 1);
  BOOST_CHECK_EQUAL(this->transport->getCounters().nInBytes, pkt1.size());

  auto pkt2 = ndn::encoding::makeStringBlock(301, "world!");
  ndn::Buffer buf2(pkt2.begin(), pkt2.end());
  this->remoteWrite(buf2);

  BOOST_CHECK_EQUAL(this->transport->getCounters().nInPackets, 2);
  BOOST_CHECK_EQUAL(this->transport->getCounters().nInBytes, pkt1.size() + pkt2.size());
  BOOST_CHECK_EQUAL(this->transport->getState(), TransportState::UP);

  BOOST_REQUIRE_EQUAL(this->receivedPackets->size(), 2);
  BOOST_CHECK(this->receivedPackets->at(0).packet == pkt1);
  BOOST_CHECK(this->receivedPackets->at(1).packet == pkt2);
  BOOST_CHECK_EQUAL(this->receivedPackets->at(0).endpoint, 0);
  BOOST_CHECK_EQUAL(this->receivedPackets->at(1).endpoint, 0);
}

BOOST_FIXTURE_TEST_CASE_TEMPLATE(ReceiveMultipleSegments, T, StreamTransportFixtures, T)
{
  TRANSPORT_TEST_INIT();

  auto pkt = ndn::encoding::makeStringBlock(300, "hello");
  ndn::Buffer buf1(pkt.begin(), pkt.end() - 2);
  ndn::Buffer buf2(pkt.end() - 2, pkt.end());

  this->remoteWrite(buf1);

  BOOST_CHECK_EQUAL(this->transport->getCounters().nInPackets, 0);
  BOOST_CHECK_EQUAL(this->transport->getCounters().nInBytes, 0);
  BOOST_CHECK_EQUAL(this->receivedPackets->size(), 0);
  BOOST_CHECK_EQUAL(this->transport->getState(), TransportState::UP);

  this->remoteWrite(buf2);

  BOOST_CHECK_EQUAL(this->transport->getCounters().nInPackets, 1);
  BOOST_CHECK_EQUAL(this->transport->getCounters().nInBytes, pkt.size());
  BOOST_CHECK_EQUAL(this->receivedPackets->size(), 1);
  BOOST_CHECK_EQUAL(this->transport->getState(), TransportState::UP);
}

BOOST_FIXTURE_TEST_CASE_TEMPLATE(ReceiveMultipleBlocks, T, StreamTransportFixtures, T)
{
  TRANSPORT_TEST_INIT();

  auto pkt1 = ndn::encoding::makeStringBlock(300, "hello");
  auto pkt2 = ndn::encoding::makeStringBlock(301, "world");
  ndn::Buffer buf(pkt1.size() + pkt2.size());
  std::copy(pkt1.begin(), pkt1.end(), buf.begin());
  std::copy(pkt2.begin(), pkt2.end(), buf.begin() + pkt1.size());

  this->remoteWrite(buf);

  BOOST_CHECK_EQUAL(this->transport->getCounters().nInPackets, 2);
  BOOST_CHECK_EQUAL(this->transport->getCounters().nInBytes, buf.size());
  BOOST_CHECK_EQUAL(this->receivedPackets->size(), 2);
  BOOST_CHECK_EQUAL(this->transport->getState(), TransportState::UP);
}

BOOST_FIXTURE_TEST_CASE_TEMPLATE(ReceiveTooLarge, T, StreamTransportFixtures, T)
{
  TRANSPORT_TEST_INIT();

  const std::vector<uint8_t> bytes(ndn::MAX_NDN_PACKET_SIZE, 0);
  auto pkt1 = ndn::encoding::makeBinaryBlock(300, ndn::make_span(bytes).subspan(6));
  ndn::Buffer buf1(pkt1.begin(), pkt1.end());
  BOOST_REQUIRE_EQUAL(buf1.size(), ndn::MAX_NDN_PACKET_SIZE);

  auto pkt2 = ndn::encoding::makeBinaryBlock(301, bytes);
  ndn::Buffer buf2(pkt2.begin(), pkt2.end());
  BOOST_REQUIRE_GT(buf2.size(), ndn::MAX_NDN_PACKET_SIZE);

  this->remoteWrite(buf1); // this should succeed

  BOOST_CHECK_EQUAL(this->transport->getCounters().nInPackets, 1);
  BOOST_CHECK_EQUAL(this->transport->getCounters().nInBytes, buf1.size());
  BOOST_CHECK_EQUAL(this->receivedPackets->size(), 1);
  BOOST_CHECK_EQUAL(this->transport->getState(), TransportState::UP);

  int nStateChanges = 0;
  this->transport->afterStateChange.connect(
    [&nStateChanges] (auto oldState, auto newState) {
      switch (nStateChanges) {
      case 0:
        BOOST_CHECK_EQUAL(oldState, TransportState::UP);
        BOOST_CHECK_EQUAL(newState, TransportState::FAILED);
        break;
      case 1:
        BOOST_CHECK_EQUAL(oldState, TransportState::FAILED);
        BOOST_CHECK_EQUAL(newState, TransportState::CLOSED);
        break;
      default:
        BOOST_CHECK(false);
      }
      nStateChanges++;
    });

  this->remoteWrite(buf2, false); // this should fail

  BOOST_CHECK_EQUAL(nStateChanges, 2);

  BOOST_CHECK_EQUAL(this->transport->getCounters().nInPackets, 1);
  BOOST_CHECK_EQUAL(this->transport->getCounters().nInBytes, buf1.size());
  BOOST_CHECK_EQUAL(this->receivedPackets->size(), 1);
}

BOOST_FIXTURE_TEST_CASE_TEMPLATE(Close, T, StreamTransportFixtures, T)
{
  TRANSPORT_TEST_INIT();

  this->transport->afterStateChange.connectSingleShot([] (auto oldState, auto newState) {
    BOOST_CHECK_EQUAL(oldState, TransportState::UP);
    BOOST_CHECK_EQUAL(newState, TransportState::CLOSING);
  });

  this->transport->close();

  this->transport->afterStateChange.connectSingleShot([this] (auto oldState, auto newState) {
    BOOST_CHECK_EQUAL(oldState, TransportState::CLOSING);
    BOOST_CHECK_EQUAL(newState, TransportState::CLOSED);
    this->limitedIo.afterOp();
  });

  BOOST_REQUIRE_EQUAL(this->limitedIo.run(1, 1_s), LimitedIo::EXCEED_OPS);
}

BOOST_FIXTURE_TEST_CASE_TEMPLATE(RemoteClose, T, StreamTransportFixtures, T)
{
  TRANSPORT_TEST_INIT();

  this->transport->afterStateChange.connectSingleShot([this] (auto oldState, auto newState) {
    BOOST_CHECK_EQUAL(oldState, TransportState::UP);
    BOOST_CHECK_EQUAL(newState, TransportState::CLOSING);
    this->limitedIo.afterOp();
  });

  this->remoteSocket.close();
  BOOST_REQUIRE_EQUAL(this->limitedIo.run(1, 1_s), LimitedIo::EXCEED_OPS);

  this->transport->afterStateChange.connectSingleShot([this] (auto oldState, auto newState) {
    BOOST_CHECK_EQUAL(oldState, TransportState::CLOSING);
    BOOST_CHECK_EQUAL(newState, TransportState::CLOSED);
    this->limitedIo.afterOp();
  });

  BOOST_REQUIRE_EQUAL(this->limitedIo.run(1, 1_s), LimitedIo::EXCEED_OPS);
}

BOOST_FIXTURE_TEST_CASE_TEMPLATE(SendQueueLength, T, StreamTransportFixtures, T)
{
  TRANSPORT_TEST_INIT();

  BOOST_CHECK_EQUAL(this->transport->getSendQueueLength(), 0);
}

BOOST_AUTO_TEST_SUITE_END() // TestStreamTransport
BOOST_AUTO_TEST_SUITE_END() // Face

} // namespace nfd::tests
