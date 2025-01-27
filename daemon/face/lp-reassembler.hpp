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

#ifndef NFD_DAEMON_FACE_LP_REASSEMBLER_HPP
#define NFD_DAEMON_FACE_LP_REASSEMBLER_HPP

#include "face-common.hpp"

#include <ndn-cxx/lp/packet.hpp>

namespace nfd::face {

/**
 * \brief Reassembles fragmented network-layer packets
 * \sa https://redmine.named-data.net/projects/nfd/wiki/NDNLPv2
 */
class LpReassembler : noncopyable
{
public:
  /** \brief Options that control the behavior of LpReassembler
   */
  struct Options
  {
    /** \brief maximum number of fragments in a packet
     *
     *  LpPackets with FragCount over this limit are dropped.
     */
    size_t nMaxFragments = 400;

    /** \brief timeout before a partially reassembled packet is dropped
     */
    time::nanoseconds reassemblyTimeout = 500_ms;
  };

  explicit
  LpReassembler(const Options& options, const LinkService* linkService = nullptr);

  /** \brief set options for reassembler
   */
  void
  setOptions(const Options& options);

  /** \return LinkService that owns this instance
   *
   *  This is only used for logging, and may be nullptr.
   */
  const LinkService*
  getLinkService() const;

  /** \brief adds received fragment to the buffer
   *  \param remoteEndpoint endpoint that sent the packet
   *  \param packet received fragment; must have Fragment field
   *  \return a tuple containing:
   *          whether a network-layer packet has been completely received,
   *          the reassembled network-layer packet,
   *          the first fragment for inspecting other NDNLPv2 headers
   *  \throw tlv::Error packet is malformed
   */
  std::tuple<bool, Block, lp::Packet>
  receiveFragment(EndpointId remoteEndpoint, const lp::Packet& packet);

  /** \brief count of partial packets
   */
  size_t
  size() const;

  /** \brief signals before a partial packet is dropped due to timeout
   *
   *  If a partial packet is incomplete and no new fragment is received
   *  within Options::reassemblyTimeout, it would be dropped due to timeout.
   *  Before it's erased, this signal is emitted with the remote endpoint,
   *  and the number of fragments being dropped.
   */
  signal::Signal<LpReassembler, EndpointId, size_t> beforeTimeout;

private:
  /**
   * \brief Holds all fragments of packet until reassembled
   */
  struct PartialPacket
  {
    std::vector<lp::Packet> fragments;
    size_t fragCount; ///< total fragments
    size_t nReceivedFragments; ///< number of received fragments
    scheduler::ScopedEventId dropTimer;
  };

  /**
   * \brief Index key for PartialPackets
   */
  using Key = std::tuple<
    EndpointId, // remoteEndpoint
    lp::Sequence // message identifier (sequence of the first fragment)
  >;

  Block
  doReassembly(const Key& key);

  void
  timeoutPartialPacket(const Key& key);

private:
  Options m_options;
  const LinkService* m_linkService;
  std::map<Key, PartialPacket> m_partialPackets;
};

std::ostream&
operator<<(std::ostream& os, const FaceLogHelper<LpReassembler>& flh);

inline void
LpReassembler::setOptions(const Options& options)
{
  m_options = options;
}

inline const LinkService*
LpReassembler::getLinkService() const
{
  return m_linkService;
}

inline size_t
LpReassembler::size() const
{
  return m_partialPackets.size();
}

} // namespace nfd::face

#endif // NFD_DAEMON_FACE_LP_REASSEMBLER_HPP
