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

#ifndef NFD_DAEMON_TABLE_PIT_FACE_RECORD_HPP
#define NFD_DAEMON_TABLE_PIT_FACE_RECORD_HPP

#include "face/face.hpp"
#include "strategy-info-host.hpp"

namespace nfd::pit {

/** \brief Contains information about an Interest on an incoming or outgoing face
 *  \note This is an implementation detail to extract common functionality
 *        of InRecord and OutRecord
 */
class FaceRecord : public StrategyInfoHost
{
public:
  explicit
  FaceRecord(Face& face)
    : m_face(face)
  {
  }

  Face&
  getFace() const
  {
    return m_face;
  }

  Interest::Nonce
  getLastNonce() const
  {
    return m_lastNonce;
  }

  time::steady_clock::TimePoint
  getLastRenewed() const
  {
    return m_lastRenewed;
  }

  /** \brief Returns the time point at which this record expires
   *  \return getLastRenewed() + InterestLifetime
   */
  time::steady_clock::TimePoint
  getExpiry() const
  {
    return m_expiry;
  }

  /** \brief updates lastNonce, lastRenewed, expiry fields
   */
  void
  update(const Interest& interest);

private:
  Face& m_face;
  Interest::Nonce m_lastNonce{0, 0, 0, 0};
  time::steady_clock::TimePoint m_lastRenewed = time::steady_clock::TimePoint::min();
  time::steady_clock::TimePoint m_expiry = time::steady_clock::TimePoint::min();
};

} // namespace nfd::pit

#endif // NFD_DAEMON_TABLE_PIT_FACE_RECORD_HPP
