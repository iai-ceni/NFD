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

#include "pit-face-record.hpp"

namespace nfd::pit {

// Impose a maximum lifetime to prevent integer overflow when calculating m_expiry.
const time::milliseconds MAX_LIFETIME = 10_days;

void
FaceRecord::update(const Interest& interest)
{
  m_lastNonce = interest.getNonce();
  m_lastRenewed = time::steady_clock::now();

  auto lifetime = interest.getInterestLifetime();
  lifetime = std::min(lifetime, MAX_LIFETIME);
  m_expiry = m_lastRenewed + lifetime;
}

} // namespace nfd::pit
