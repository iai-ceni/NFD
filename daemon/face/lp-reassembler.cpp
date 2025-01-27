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

#include "lp-reassembler.hpp"
#include "link-service.hpp"
#include "common/global.hpp"

#include <numeric>

namespace nfd::face {

NFD_LOG_INIT(LpReassembler);

LpReassembler::LpReassembler(const LpReassembler::Options& options, const LinkService* linkService)
  : m_options(options)
  , m_linkService(linkService)
{
}

std::tuple<bool, Block, lp::Packet>
LpReassembler::receiveFragment(EndpointId remoteEndpoint, const lp::Packet& packet)
{
  BOOST_ASSERT(packet.has<lp::FragmentField>());

  // read and check FragIndex and FragCount
  uint64_t fragIndex = 0;
  uint64_t fragCount = 1;
  if (packet.has<lp::FragIndexField>()) {
    fragIndex = packet.get<lp::FragIndexField>();
  }
  if (packet.has<lp::FragCountField>()) {
    fragCount = packet.get<lp::FragCountField>();
  }

  if (fragIndex >= fragCount) {
    NFD_LOG_FACE_WARN("reassembly error, FragIndex>=FragCount: DROP");
    return {false, {}, {}};
  }

  if (fragCount > m_options.nMaxFragments) {
    NFD_LOG_FACE_WARN("reassembly error, FragCount over limit: DROP");
    return {false, {}, {}};
  }

  // check for fast path
  if (fragIndex == 0 && fragCount == 1) {
    auto frag = packet.get<lp::FragmentField>();
    Block netPkt({frag.first, frag.second});
    return {true, netPkt, packet};
  }

  // check Sequence and compute message identifier
  if (!packet.has<lp::SequenceField>()) {
    NFD_LOG_FACE_WARN("reassembly error, Sequence missing: DROP");
    return {false, {}, {}};
  }

  lp::Sequence messageIdentifier = packet.get<lp::SequenceField>() - fragIndex;
  Key key(remoteEndpoint, messageIdentifier);

  // add to PartialPacket
  PartialPacket& pp = m_partialPackets[key];
  if (pp.fragCount == 0) { // new PartialPacket
    pp.fragCount = fragCount;
    pp.nReceivedFragments = 0;
    pp.fragments.resize(fragCount);
  }
  else {
    if (fragCount != pp.fragCount) {
      NFD_LOG_FACE_WARN("reassembly error, FragCount changed: DROP");
      return {false, {}, {}};
    }
  }

  if (pp.fragments[fragIndex].has<lp::SequenceField>()) {
    NFD_LOG_FACE_TRACE("fragment already received: DROP");
    return {false, {}, {}};
  }

  pp.fragments[fragIndex] = packet;
  ++pp.nReceivedFragments;

  // check complete condition
  if (pp.nReceivedFragments == pp.fragCount) {
    Block reassembled = doReassembly(key);
    lp::Packet firstFrag(std::move(pp.fragments[0]));
    m_partialPackets.erase(key);
    return {true, reassembled, firstFrag};
  }

  // set drop timer
  pp.dropTimer = getScheduler().schedule(m_options.reassemblyTimeout, [=] { timeoutPartialPacket(key); });

  return {false, {}, {}};
}

Block
LpReassembler::doReassembly(const Key& key)
{
  PartialPacket& pp = m_partialPackets[key];

  size_t payloadSize = std::accumulate(pp.fragments.begin(), pp.fragments.end(), 0U,
    [&] (size_t sum, const lp::Packet& pkt) -> size_t {
      auto [fragBegin, fragEnd] = pkt.get<lp::FragmentField>();
      return sum + std::distance(fragBegin, fragEnd);
    });

  ndn::Buffer fragBuffer(payloadSize);
  auto it = fragBuffer.begin();
  for (const lp::Packet& frag : pp.fragments) {
    auto [fragBegin, fragEnd] = frag.get<lp::FragmentField>();
    it = std::copy(fragBegin, fragEnd, it);
  }
  return Block(fragBuffer);
}

void
LpReassembler::timeoutPartialPacket(const Key& key)
{
  auto it = m_partialPackets.find(key);
  if (it == m_partialPackets.end()) {
    return;
  }

  this->beforeTimeout(std::get<0>(key), it->second.nReceivedFragments);
  m_partialPackets.erase(it);
}

std::ostream&
operator<<(std::ostream& os, const FaceLogHelper<LpReassembler>& flh)
{
  if (flh.obj.getLinkService() == nullptr) {
    os << "[id=0,local=unknown,remote=unknown] ";
  }
  else {
    os << FaceLogHelper<LinkService>(*flh.obj.getLinkService());
  }
  return os;
}

} // namespace nfd::face
