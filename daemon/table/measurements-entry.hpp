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

#ifndef NFD_DAEMON_TABLE_MEASUREMENTS_ENTRY_HPP
#define NFD_DAEMON_TABLE_MEASUREMENTS_ENTRY_HPP

#include "strategy-info-host.hpp"

namespace nfd::name_tree {
class Entry;
} // namespace nfd::name_tree

namespace nfd::measurements {

class Measurements;

/**
 * \brief Represents an entry in the %Measurements table.
 * \sa Measurements
 */
class Entry : public StrategyInfoHost, noncopyable
{
public:
  explicit
  Entry(const Name& name)
    : m_name(name)
  {
  }

  const Name&
  getName() const
  {
    return m_name;
  }

private:
  Name m_name;
  time::steady_clock::TimePoint m_expiry = time::steady_clock::TimePoint::min();
  scheduler::EventId m_cleanup;

  name_tree::Entry* m_nameTreeEntry = nullptr;

  friend ::nfd::name_tree::Entry;
  friend Measurements;
};

} // namespace nfd::measurements

#endif // NFD_DAEMON_TABLE_MEASUREMENTS_ENTRY_HPP
