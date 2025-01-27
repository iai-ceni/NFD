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

#include "face/internal-face.hpp"

#include "transport-test-common.hpp"
#include "tests/key-chain-fixture.hpp"
#include "tests/daemon/global-io-fixture.hpp"

namespace nfd::tests {

using namespace nfd::face;

BOOST_AUTO_TEST_SUITE(Face)

class InternalFaceFixture : public GlobalIoTimeFixture, public KeyChainFixture
{
public:
  InternalFaceFixture()
  {
    std::tie(forwarderFace, clientFace) = makeInternalFace(m_keyChain);

    forwarderFace->afterReceiveInterest.connect(
      [this] (const Interest& interest, const EndpointId&) { receivedInterests.push_back(interest); } );
    forwarderFace->afterReceiveData.connect(
      [this] (const Data& data, const EndpointId&) { receivedData.push_back(data); } );
    forwarderFace->afterReceiveNack.connect(
      [this] (const lp::Nack& nack, const EndpointId&) { receivedNacks.push_back(nack); } );
  }

protected:
  shared_ptr<nfd::Face> forwarderFace;
  shared_ptr<ndn::Face> clientFace;

  std::vector<Interest> receivedInterests;
  std::vector<Data> receivedData;
  std::vector<lp::Nack> receivedNacks;
};

BOOST_FIXTURE_TEST_SUITE(TestInternalFace, InternalFaceFixture)

BOOST_AUTO_TEST_CASE(TransportStaticProperties)
{
  Transport* transport = forwarderFace->getTransport();
  checkStaticPropertiesInitialized(*transport);

  BOOST_CHECK_EQUAL(transport->getLocalUri(), FaceUri("internal://"));
  BOOST_CHECK_EQUAL(transport->getRemoteUri(), FaceUri("internal://"));
  BOOST_CHECK_EQUAL(transport->getScope(), ndn::nfd::FACE_SCOPE_LOCAL);
  BOOST_CHECK_EQUAL(transport->getPersistency(), ndn::nfd::FACE_PERSISTENCY_PERMANENT);
  BOOST_CHECK_EQUAL(transport->getLinkType(), ndn::nfd::LINK_TYPE_POINT_TO_POINT);
  BOOST_CHECK_EQUAL(transport->getMtu(), MTU_UNLIMITED);
}

// note: "send" and "receive" in test case names refer to the direction seen on forwarderFace.
// i.e. "send" means transmission from forwarder to client,
//      "receive" means transmission from client to forwarder.

BOOST_AUTO_TEST_CASE(ReceiveInterestTimeout)
{
  auto interest = makeInterest("/TLETccRv");
  interest->setInterestLifetime(100_ms);

  bool hasTimeout = false;
  clientFace->expressInterest(*interest,
    [] (auto&&...) { BOOST_ERROR("unexpected Data"); },
    [] (auto&&...) { BOOST_ERROR("unexpected Nack"); },
    [&] (auto&&...) { hasTimeout = true; });
  this->advanceClocks(1_ms, 10);

  BOOST_REQUIRE_EQUAL(receivedInterests.size(), 1);
  BOOST_CHECK_EQUAL(receivedInterests.back().getName(), "/TLETccRv");

  this->advanceClocks(1_ms, 100);

  BOOST_CHECK(hasTimeout);
}

BOOST_AUTO_TEST_CASE(ReceiveInterestSendData)
{
  auto interest = makeInterest("/PQstEJGdL", true);

  bool hasReceivedData = false;
  clientFace->expressInterest(*interest,
    [&] (const Interest&, const Data& data) {
      hasReceivedData = true;
      BOOST_CHECK_EQUAL(data.getName(), "/PQstEJGdL/aI7oCrDXNX");
    },
    [] (auto&&...) { BOOST_ERROR("unexpected Nack"); },
    [] (auto&&...) { BOOST_ERROR("unexpected timeout"); });
  this->advanceClocks(1_ms, 10);

  BOOST_REQUIRE_EQUAL(receivedInterests.size(), 1);
  BOOST_CHECK_EQUAL(receivedInterests.back().getName(), "/PQstEJGdL");

  forwarderFace->sendData(*makeData("/PQstEJGdL/aI7oCrDXNX"));
  this->advanceClocks(1_ms, 10);

  BOOST_CHECK(hasReceivedData);
}

BOOST_AUTO_TEST_CASE(ReceiveInterestSendNack)
{
  auto interest = makeInterest("/1HrsRM1X");

  bool hasReceivedNack = false;
  clientFace->expressInterest(*interest,
    [] (auto&&...) { BOOST_ERROR("unexpected Data"); },
    [&] (const Interest&, const lp::Nack& nack) {
      hasReceivedNack = true;
      BOOST_CHECK_EQUAL(nack.getReason(), lp::NackReason::NO_ROUTE);
    },
    [] (auto&&...) { BOOST_ERROR("unexpected timeout"); });
  this->advanceClocks(1_ms, 10);

  BOOST_REQUIRE_EQUAL(receivedInterests.size(), 1);
  BOOST_CHECK_EQUAL(receivedInterests.back().getName(), "/1HrsRM1X");

  forwarderFace->sendNack(makeNack(*interest, lp::NackReason::NO_ROUTE));
  this->advanceClocks(1_ms, 10);

  BOOST_CHECK(hasReceivedNack);
}

BOOST_AUTO_TEST_CASE(SendInterestReceiveData)
{
  bool hasDeliveredInterest = false;
  clientFace->setInterestFilter("/Wpc8TnEeoF",
    [this, &hasDeliveredInterest] (const ndn::InterestFilter&, const Interest& interest) {
      hasDeliveredInterest = true;
      BOOST_CHECK_EQUAL(interest.getName(), "/Wpc8TnEeoF/f6SzV8hD");

      clientFace->put(*makeData("/Wpc8TnEeoF/f6SzV8hD/3uytUJCuIi"));
    });

  forwarderFace->sendInterest(*makeInterest("/Wpc8TnEeoF/f6SzV8hD", true));
  this->advanceClocks(1_ms, 10);

  BOOST_CHECK(hasDeliveredInterest);
  BOOST_REQUIRE_EQUAL(receivedData.size(), 1);
  BOOST_CHECK_EQUAL(receivedData.back().getName(), "/Wpc8TnEeoF/f6SzV8hD/3uytUJCuIi");
}

BOOST_AUTO_TEST_CASE(SendInterestReceiveNack)
{
  auto interest = makeInterest("/4YgJKWcXN/5oaTe05o");

  bool hasDeliveredInterest = false;
  clientFace->setInterestFilter("/4YgJKWcXN",
    [this, &hasDeliveredInterest] (const ndn::InterestFilter&, const Interest& interest) {
      hasDeliveredInterest = true;
      BOOST_CHECK_EQUAL(interest.getName(), "/4YgJKWcXN/5oaTe05o");
      clientFace->put(makeNack(interest, lp::NackReason::NO_ROUTE));
    });

  forwarderFace->sendInterest(*interest);
  this->advanceClocks(1_ms, 10);

  BOOST_CHECK(hasDeliveredInterest);
  BOOST_REQUIRE_EQUAL(receivedNacks.size(), 1);
  BOOST_CHECK_EQUAL(receivedNacks.back().getReason(), lp::NackReason::NO_ROUTE);
}

BOOST_AUTO_TEST_CASE(CloseForwarderFace)
{
  forwarderFace->close();
  this->advanceClocks(1_ms, 10);
  BOOST_CHECK_EQUAL(forwarderFace->getState(), FaceState::CLOSED);
  forwarderFace.reset();

  auto interest = makeInterest("/zpHsVesu0B");
  interest->setInterestLifetime(100_ms);

  bool hasTimeout = false;
  clientFace->expressInterest(*interest,
    [] (auto&&...) { BOOST_ERROR("unexpected Data"); },
    [] (auto&&...) { BOOST_ERROR("unexpected Nack"); },
    [&] (auto&&...) { hasTimeout = true; });
  BOOST_CHECK_NO_THROW(this->advanceClocks(1_ms, 200));

  BOOST_CHECK_EQUAL(receivedInterests.size(), 0);
  BOOST_CHECK(hasTimeout);
}

BOOST_AUTO_TEST_CASE(CloseClientFace)
{
  g_io.poll(); // #3248 workaround
  clientFace.reset();

  forwarderFace->sendInterest(*makeInterest("/aau42XQqb"));
  BOOST_CHECK_NO_THROW(this->advanceClocks(1_ms, 10));
}

BOOST_AUTO_TEST_SUITE_END() // TestInternalFace
BOOST_AUTO_TEST_SUITE_END() // Face

} // namespace nfd::tests
