/**
 * @file DQMProcessor.cpp DQMProcessor Class Implementation
 *
 * See header for more on this class
 *
 * This is part of the DUNE DAQ Application Framework, copyright 2020.
 * Licensing/copyright details are in the COPYING file that you should have
 * received with this code.
 */
// DQM includes
#include "dqm/Types.hpp"
#include "dqm/dqmprocessor/Nljs.hpp"
#include "dqm/dqmprocessorinfo/InfoNljs.hpp"
#include "dqm/dqmprocessor/Structs.hpp"

#include "DQMProcessor.hpp"
#include "HistContainer.hpp"
#include "FourierContainer.hpp"

#include "appfwk/DAQSource.hpp"
#include "dataformats/ComponentRequest.hpp"
#include "dataformats/Fragment.hpp"
#include "dataformats/GeoID.hpp"
#include "dataformats/TriggerRecord.hpp"
#include "dataformats/wib/WIBFrame.hpp"
#include "dfmessages/TriggerDecision.hpp"

#include <chrono>
#include <map>
#include <memory>
#include <string>
#include <thread>
#include <vector>

namespace dunedaq {
namespace dqm {

DQMProcessor::DQMProcessor(const std::string& name)
  : DAQModule(name)
{
  register_command("start", &DQMProcessor::do_start);
  register_command("conf", &DQMProcessor::do_configure);
  register_command("stop", &DQMProcessor::do_stop);
}

void
DQMProcessor::init(const data_t&)
{
  m_source.reset(new trigger_record_source_qt("trigger_record_q_dqm"));
  m_sink.reset(new trigger_decision_sink_qt("trigger_decision_q_dqm"));
  m_timesync_source.reset(new timesync_source_qt("time_sync_dqm_q"));
}

void
DQMProcessor::get_info(opmonlib::InfoCollector& ci, int /*level*/)
{
  dqmprocessorinfo::Info fcr;

  fcr.requests = m_request_count.exchange(0);
  fcr.total_requests = m_total_request_count.load();
  fcr.data_deliveries = m_data_count.exchange(0);
  fcr.total_data_deliveries = m_total_data_count.load();

  ci.add(fcr);
}

void
DQMProcessor::do_configure(const nlohmann::json& args)
{
  auto conf = args.get<dqmprocessor::Conf>();
  if (conf.mode == "debug" || conf.mode == "local processing") {
    m_running_mode = RunningMode::kLocalProcessing;
  } else if (conf.mode == "normal") {
    m_running_mode = RunningMode::kNormal;
  } else {
    TLOG() << "Invalid value for mode, supported values are 'debug', 'local processing' and 'normal'";
  }
  // m_source = std::unique_ptr<appfwk::DAQSource < std::unique_ptr<dataformats::TriggerRecord >>>
  // ("trigger_record_q_dqm"); m_sink = std::unique_ptr<appfwk::DAQSink < dfmessages::TriggerDecision >>
  // ("trigger_decision_q_dqm");
  m_kafka_address = conf.kafka_address;
  m_standard_dqm_hist = conf.sdqm_hist;
  m_standard_dqm_fourier = conf.sdqm_fourier;

  m_link_idx = conf.link_idx;

  m_clock_frequency = conf.clock_frequency;
}

void
DQMProcessor::do_start(const nlohmann::json& args)
{
  m_time_est.reset(new timinglibs::TimestampEstimator(m_timesync_source, m_clock_frequency));

  m_run_marker.store(true);

  m_run_number.store(dataformats::run_number_t(
      args.at("run").get<dataformats::run_number_t>()));

  m_running_thread.reset(new std::thread(&DQMProcessor::RequestMaker, this));
}

void
DQMProcessor::do_stop(const data_t&)
{
  m_run_marker.store(false);
  m_running_thread->join();
  // Delete the timestamp estimator
  // Since it's not a plugin it runs forever until deleted
  // m_time_est.reset(nullptr);
}

void
DQMProcessor::RequestMaker()
{

  // Helper struct with the necessary information about an instance
  struct AnalysisInstance
  {
    AnalysisModule* mod;
    double between_time;
    double default_unavailable_time;
    std::thread* running_thread;
    std::string name;
  };

  // For now only one link
  std::vector<dataformats::GeoID> m_links;

  for (auto i: m_link_idx) {
    m_links.push_back({ dataformats::GeoID::SystemType::kTPC, 0, static_cast<unsigned int>(i) });
  }

  std::map<std::chrono::time_point<std::chrono::system_clock>, AnalysisInstance> map;

  std::unique_ptr<dataformats::TriggerRecord> element;

  // Instances of analysis modules
  HistContainer hist("hist1s", 256, 100, 0, 5000);
  FourierContainer fourier("fourier10s", 256, 25, 100);

  // Initial tasks
  map[std::chrono::system_clock::now()] = {&hist, m_standard_dqm_hist.how_often, m_standard_dqm_hist.unavailable_time, nullptr, "hist"};
  map[std::chrono::system_clock::now()] = {&fourier, m_standard_dqm_fourier.how_often, m_standard_dqm_fourier.unavailable_time, nullptr, "fourier"};

  // Main loop, running forever
  while (m_run_marker) {

    auto fr = map.begin();
    if (fr == map.end()) {
      TLOG() << "Empty map! This should never happen!";
      break;
    }
    auto next_time = fr->first;
    AnalysisModule* algo = fr->second.mod;

    // Save pointer to delete the thread later
    std::thread* previous_thread = fr->second.running_thread;

    // TLOG() << "TIME: next_time" << next_time.time_since_epoch().count();

    // TLOG() << "MAIN LOOP" << " Instance of " << fr->second.between_time << " seconds" << (algo == &hist1s) << " " <<
    // (algo == &hist5s);

    // Sleep until the next time
    std::this_thread::sleep_until(next_time);
    // We don't want to run if the run has stopped
    if (!m_run_marker) {
      break;
    }


    // Make sure that the process is not running and a request can be made
    // otherwise we wait for more time
    if (algo->is_running()) {
      TLOG() << "ALGORITHM already running";
      map[std::chrono::system_clock::now() +
          std::chrono::milliseconds(static_cast<int>(fr->second.default_unavailable_time) * 1000)] = {
        algo, fr->second.between_time, fr->second.default_unavailable_time, previous_thread, fr->second.name};
      map.erase(fr);
      continue;
    }
    // Before creating a request check that there
    // There has been a bug where the timestamp was retrieved before there were any timestamps
    // obtaining an invalid timestamps

    auto timestamp = m_time_est->get_timestamp_estimate();
    if (timestamp == dfmessages::TypeDefaults::s_invalid_timestamp) {
      // Some sleep is needed because at the beginning there are no valid timestamps
      // so it will be checking continuously if there is a valid one
      std::this_thread::sleep_for(std::chrono::milliseconds(10));
      continue;
    }

    // Now it's the time to do something

    int num_frames = 1;
    if (fr->second.name == "hist") num_frames = m_standard_dqm_hist.num_frames;
    else if (fr->second.name == "fourier") num_frames = m_standard_dqm_fourier.num_frames;
    auto request = CreateRequest(m_links, num_frames);

    try {
      m_sink->push(request, m_sink_timeout);
    } catch (const ers::Issue &excpt) {
      TLOG() << "DQM: Unable to push to the request queue";
      continue;
    }
    ++m_request_count;
    ++m_total_request_count;

    TLOG_DEBUG(10) << "Request (trigger decision) pushed to the queue";

    // TLOG() << "Going to pop";
    try {
      m_source->pop(element, m_source_timeout);
    } catch (const ers::Issue &excpt) {
      TLOG() << "DQM: Unable to pop from the data queue";
      continue;
    }
    ++m_data_count;
    ++m_total_data_count;

    TLOG_DEBUG(10) << "Data popped from the queue";

    std::thread* current_thread =
      new std::thread(&AnalysisModule::run, std::ref(*algo), std::ref(*element), m_running_mode, m_kafka_address);

    // Add a new entry for the current instance
    map[std::chrono::system_clock::now() +
        std::chrono::milliseconds(static_cast<int>(fr->second.between_time) * 1000)] = {
      algo, fr->second.between_time, fr->second.default_unavailable_time, current_thread, fr->second.name
    };

    // Delete the entry we just used and find the next one
    map.erase(fr);

    // Delete thread
    if (previous_thread != nullptr) {
      if (previous_thread->joinable()) {
        previous_thread->join();
        delete previous_thread; // TODO: rsipos -> Why is this a raw pointer on the thread? Move to unique_ptr.
      } else {
        // Should not be happening
      }
    }
  }
  // Delete the timestamp estimator after we are sure we won't need it
  m_time_est.reset(nullptr);
}

dfmessages::TriggerDecision
DQMProcessor::CreateRequest(std::vector<dfmessages::GeoID> m_links, int num_frames)
{
  auto timestamp = m_time_est->get_timestamp_estimate();
  dfmessages::TriggerDecision decision;

  static dataformats::trigger_number_t trigger_number = 1;

  decision.trigger_number = trigger_number++;
  decision.run_number = m_run_number;
  decision.trigger_timestamp = timestamp;
  decision.readout_type = dfmessages::ReadoutType::kMonitoring;

  int number_of_frames = num_frames;
  int window_size = number_of_frames * 25;

  for (auto& link : m_links) {
    // TLOG() << "ONE LINK";
    dataformats::ComponentRequest request;
    request.component = link;
    // Some offset is required to avoid having delayed requests in readout
    // which make the TRB to take longer and longer to create the trigger records
    // 10^5 was tried at first but wasn't enough. At 50 MHz, 10^5 = 2 ms
    // The current value is 10^7 which seems to work after several hours of
    // running and no delayed requests in readout
    request.window_begin = timestamp - window_size - 10000000;
    request.window_end = timestamp - 10000000;

    decision.components.push_back(request);
  }

  TLOG_DEBUG(10) << "Making request (trigger decision) asking for " << m_links.size() <<
    " links and with the beginning of the window at timestamp " <<
    timestamp - window_size << " and the end of the window at timestamp " << timestamp;
    ;

  return decision;
}

} // namespace dqm
} // namespace dunedaq

// Define the module
DEFINE_DUNE_DAQ_MODULE(dunedaq::dqm::DQMProcessor)
