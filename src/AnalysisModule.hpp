/**
 * @file AnalysisModule.hpp Definition of the abstract class AnalysisModule
 *
 * This is part of the DUNE DAQ , copyright 2020.
 * Licensing/copyright details are in the COPYING file that you should have
 * received with this code.
 */
#ifndef DQM_SRC_ANALYSISMODULE_HPP_
#define DQM_SRC_ANALYSISMODULE_HPP_

#include "ChannelMap.hpp"

#include "daqdataformats/TriggerRecord.hpp"

#include <atomic>

namespace dunedaq::dqm {

class AnalysisModule
{
public:
  std::atomic<bool> m_run_mark = false;

  virtual bool is_running();
  virtual void run(std::unique_ptr<daqdataformats::TriggerRecord> record, std::unique_ptr<ChannelMap> &map, std::string kafka_address) = 0;
};

bool
AnalysisModule::is_running(){
  return m_run_mark;
}

} // namespace dunedaq::dqm

#endif // DQM_SRC_ANALYSISMODULE_HPP_
