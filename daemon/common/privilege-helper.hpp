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

#ifndef NFD_DAEMON_COMMON_PRIVILEGE_HELPER_HPP
#define NFD_DAEMON_COMMON_PRIVILEGE_HELPER_HPP

#include "core/common.hpp"

#include <unistd.h>

namespace nfd {

class PrivilegeHelper
{
public:
  /** \brief represents a serious seteuid/gid failure
   *
   *  This should only be caught by main as part of a graceful program termination.
   *  \note This is not an std::exception and NDN_THROW should not be used.
   */
  class Error
  {
  public:
    explicit
    Error(const std::string& what)
      : m_whatMessage(what)
    {
    }

    const char*
    what() const
    {
      return m_whatMessage.data();
    }

  private:
    const std::string m_whatMessage;
  };

  static void
  initialize(const std::string& userName, const std::string& groupName);

  static void
  drop();

  template<class F>
  static void
  runElevated(F&& f)
  {
    raise();
    try {
      std::invoke(std::forward<F>(f));
    }
    catch (...) {
      drop();
      throw;
    }
    drop();
  }

NFD_PUBLIC_WITH_TESTS_ELSE_PRIVATE:
  static void
  raise();

NFD_PUBLIC_WITH_TESTS_ELSE_PRIVATE:
#ifdef NFD_HAVE_PRIVILEGE_DROP_AND_ELEVATE
  static uid_t s_normalUid;
  static gid_t s_normalGid;

  static uid_t s_privilegedUid;
  static gid_t s_privilegedGid;
#endif // NFD_HAVE_PRIVILEGE_DROP_AND_ELEVATE
};

} // namespace nfd

#endif // NFD_DAEMON_COMMON_PRIVILEGE_HELPER_HPP
