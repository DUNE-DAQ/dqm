/**
 * @file DQMProcessor.hpp DQMProcessor Class Interface
 *
 * DQMProcessor is a class that illustrates how to make a DUNE DAQ
 * module that interacts with another one. It does some printing to the screen
 * and prints the trigger number of a Fragment
 *
 * This is part of the DUNE DAQ Application Framework, copyright 2020.
 * Licensing/copyright details are in the COPYING file that you should have
 * received with this code.
 */
#ifndef DQM_PLUGINS_DQMPROCESSOR_HPP_
#define DQM_PLUGINS_DQMPROCESSOR_HPP_

#include "ChannelMap.hpp"

#include "appfwk/DAQModule.hpp"
#include "appfwk/DAQSink.hpp"
#include "appfwk/DAQSource.hpp"
#include "daqdataformats/TriggerRecord.hpp"
#include "dfmessages/TriggerDecision.hpp"
#include "timinglibs/TimestampEstimator.hpp"

#include <atomic>
#include <chrono>
#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace dunedaq::dqm {

class DQMProcessor : public dunedaq::appfwk::DAQModule
{

public:
  /**
   * @brief DQMProcessor Constructor
   * @param name Instance name for this DQMProcessor instance
   */
  explicit DQMProcessor(const std::string& name);

  DQMProcessor(const DQMProcessor&) = delete;            ///< DQMProcessor is not copy-constructible
  DQMProcessor& operator=(const DQMProcessor&) = delete; ///< DQMProcessor is not copy-assignable
  DQMProcessor(DQMProcessor&&) = delete;                 ///< DQMProcessor is not move-constructible
  DQMProcessor& operator=(DQMProcessor&&) = delete;      ///< DQMProcessor is not move-assignable

  void init(const data_t&) override;

  void do_print(const data_t&);
  void do_start(const data_t&);
  void do_pause(const data_t&);
  void do_resume(const data_t&);
  void do_stop(const data_t&);
  void do_configure(const data_t&);

  std::atomic<bool> m_run_marker;

  void RequestMaker();
  dfmessages::TriggerDecision CreateRequest(std::vector<dfmessages::GeoID>& m_links, int number_of_frames);

  void get_info(opmonlib::InfoCollector& ci, int /*level*/);

private:
  using trigger_record_source_qt = appfwk::DAQSource<std::unique_ptr<daqdataformats::TriggerRecord>>;
  std::unique_ptr<trigger_record_source_qt> m_source;
  using trigger_decision_sink_qt = appfwk::DAQSink<dfmessages::TriggerDecision>;
  std::unique_ptr<trigger_decision_sink_qt> m_sink;

  std::chrono::milliseconds m_sink_timeout{ 1000 };
  std::chrono::milliseconds m_source_timeout{ 1000 };

  // Configuration parameters
  dqmprocessor::StandardDQM m_standard_dqm_hist;
  dqmprocessor::StandardDQM m_standard_dqm_mean_rms;
  dqmprocessor::StandardDQM m_standard_dqm_fourier;

  using timesync_source_qt = appfwk::DAQSource<dfmessages::TimeSync>;
  std::unique_ptr<timesync_source_qt> m_timesync_source;

  std::unique_ptr<timinglibs::TimestampEstimator> m_time_est;

  std::atomic<daqdataformats::run_number_t> m_run_number;

  std::string m_kafka_address;
  std::vector<int> m_link_idx;

  uint16_t m_region;
  int m_clock_frequency;

  std::unique_ptr<std::thread> m_running_thread;

  // paused state, in which we don't send triggers
  std::atomic<bool> m_paused;

  std::atomic<int> m_request_count{0};
  std::atomic<int> m_total_request_count{0};
  std::atomic<int> m_data_count{0};
  std::atomic<int> m_total_data_count{0};

  std::string m_channel_map;
  std::unique_ptr<ChannelMap> m_map;

};

} // namespace dunedaq::dqm

#endif // DQM_PLUGINS_DQMPROCESSOR_HPP_
