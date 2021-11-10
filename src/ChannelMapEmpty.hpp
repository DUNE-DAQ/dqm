/**
 * @file ChannelMapEmpty.hpp Implementation of an empty channel map for
 * initialization
 *
 * This is part of the DUNE DAQ , copyright 2020.
 * Licensing/copyright details are in the COPYING file that you should have
 * received with this code.
 */
#ifndef DQM_SRC_CHANNELMAPEMPTY_HPP_
#define DQM_SRC_CHANNELMAPEMPTY_HPP_

// DQM
#include "ChannelMap.hpp"

namespace dunedaq::dqm {

class ChannelMapEmpty : public ChannelMap{

public:
  bool is_filled();
};


bool
ChannelMapEmpty::is_filled() {
  return m_is_filled;
}

} // namespace dunedaq::dqm

#endif // DQM_SRC_CHANNELMAPEMPTY_HPP_
