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
#include "Constants.hpp"
#include "dqm/dqmprocessor/Nljs.hpp"
#include "dqm/dqmprocessor/Structs.hpp"
#include "dqm/dqmprocessorinfo/InfoNljs.hpp"

#include "ChannelMapEmpty.hpp"
#include "ChannelMapFiller.hpp"
#include "DFModule.hpp"
#include "DQMProcessor.hpp"
#include "FourierContainer.hpp"
#include "HistContainer.hpp"
#include "ChannelMask.hpp"

// DUNE-DAQ includes
#include "appfwk/DAQSource.hpp"
#include "daqdataformats/ComponentRequest.hpp"
#include "daqdataformats/Fragment.hpp"
#include "daqdataformats/GeoID.hpp"
#include "daqdataformats/TriggerRecord.hpp"
#include "detdataformats/wib/WIBFrame.hpp"
#include "dfmessages/TimeSync.hpp"
#include "dfmessages/TriggerDecision.hpp"
#include "dfmessages/TRMonRequest.hpp"
#include "networkmanager/NetworkManager.hpp"
#include "dfmessages/TriggerRecord_serialization.hpp"

// C++ includes
#include <chrono>
#include <map>
#include <memory>
#include <string>
#include <thread>
#include <utility>
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
  m_kafka_address = conf.kafka_address;

  m_mode = conf.mode;
  m_frontend_type = conf.frontend_type;

  m_standard_dqm_hist = conf.sdqm_hist;
  m_standard_dqm_mean_rms = conf.sdqm_mean_rms;
  m_standard_dqm_fourier = conf.sdqm_fourier;
  m_standard_dqm_fourier_sum = conf.sdqm_fourier_sum;
  m_standard_channel_mask = conf.sdqm_channel_mask;

  m_df_seconds = conf.df_seconds;
  m_df_offset = conf.df_offset;
  m_df_algs = conf.df_algs;
  m_df_num_frames = conf.df_num_frames;

  m_link_idx = conf.link_idx;
  m_clock_frequency = conf.clock_frequency;
  m_channel_map = conf.channel_map;
  m_region = conf.region;
  m_readout_window_offset = conf.readout_window_offset;

  m_timesync_connection = conf.timesync_connection_name;
  m_df2dqm_connection = conf.df2dqm_connection_name;
  m_dqm2df_connection = conf.dqm2df_connection_name;

  if (m_mode == "df") {
      networkmanager::NetworkManager::get().start_listening(m_df2dqm_connection);
  }
  else if (m_mode == "readout") {
    m_source.reset(new trigger_record_source_qt("trigger_record_q_dqm"));
    m_sink.reset(new trigger_decision_sink_qt("trigger_decision_q_dqm"));
  }
}

void
DQMProcessor::do_start(const nlohmann::json& args)
{
  if (m_mode == "readout") {

    m_time_est.reset(new timinglibs::TimestampEstimator(m_clock_frequency));

    m_received_timesync_count.store(0);
    networkmanager::NetworkManager::get().start_listening(m_timesync_connection);
    networkmanager::NetworkManager::get().register_callback(
      m_timesync_connection, std::bind(&DQMProcessor::dispatch_timesync, this, std::placeholders::_1));
  }

  if (m_mode == "df") {
  networkmanager::NetworkManager::get().register_callback(m_df2dqm_connection,
    std::bind(&DQMProcessor::dispatch_trigger_record, this, std::placeholders::_1));
  }

  m_run_marker.store(true);

  m_run_number.store(daqdataformats::run_number_t(args.at("run").get<daqdataformats::run_number_t>()));

  // The channel map pointer is set to the empty channel map that is not filled
  // and allows the first check to pass for it to be filled with the actual
  // channel map
  m_map = std::shared_ptr<ChannelMap>(new ChannelMapEmpty);

  m_running_thread.reset(new std::thread(&DQMProcessor::RequestMaker, this));
}

void
DQMProcessor::do_stop(const data_t&)
{
  m_run_marker.store(false);
  m_running_thread->join();

  if (m_mode == "readout") {
    networkmanager::NetworkManager::get().clear_callback(m_timesync_connection);
    networkmanager::NetworkManager::get().stop_listening(m_timesync_connection);
  }
  TLOG() << get_name() << ": received " << m_received_timesync_count.load() << " TimeSync messages.";
}

void
DQMProcessor::RequestMaker()
{

  // Helper struct with the necessary information about an instance
  struct AnalysisInstance
  {
    std::shared_ptr<AnalysisModule> mod;
    double between_time;
    double default_unavailable_time;
    int number_of_frames;
    std::shared_ptr<std::thread> running_thread;
    std::string name;
  };

  std::vector<daqdataformats::GeoID> m_links;

  for (auto i : m_link_idx) {
    m_links.push_back({ daqdataformats::GeoID::SystemType::kTPC, m_region, static_cast<unsigned int>(i) });
  }

  // Map that holds the tasks and times when to do them
  std::map<std::chrono::time_point<std::chrono::system_clock>, AnalysisInstance> map;

  std::unique_ptr<daqdataformats::TriggerRecord> element{ nullptr };

  // Instances of analysis modules

  // Raw event display
  auto hist = std::make_shared<HistContainer>(
      "raw_display", CHANNELS_PER_LINK * m_link_idx.size(), m_link_idx, 100, 0, 12000, false);
  // Mean and RMS
  auto mean_rms = std::make_shared<HistContainer>(
      "rmsm_display", CHANNELS_PER_LINK * m_link_idx.size(), m_link_idx, 100, 0, 12000, true);
  // Fourier transform
  // The Delta of time between frames is the inverse of the sampling frequency (clock frequency)
  // but because we are sampling every TICKS_BETWEEN_TIMESTAMP ticks we have to multiply by that
  auto fourier = std::make_shared<FourierContainer>("fft_display",
                                                      CHANNELS_PER_LINK * m_link_idx.size(),
                                                      m_link_idx,
                                                      1. / m_clock_frequency * TICKS_BETWEEN_TIMESTAMP,
                                                      m_standard_dqm_fourier.num_frames);
  auto fouriersum = std::make_shared<FourierContainer>("fft_sums_display",
                                                      4,
                                                      m_link_idx,
                                                      1. / m_clock_frequency * TICKS_BETWEEN_TIMESTAMP,
                                                      m_standard_dqm_fourier_sum.num_frames,
                                                      true);
  auto channel_mask = std::make_shared<ChannelMask>("channel_mask_display",
                                                    m_link_idx);

  // Whether an algorithm is enabled or not depends on the value of the bitfield m_df_algs
  TLOG() << "m_df_algs = " << m_df_algs;
  auto dfmodule = std::make_shared<DFModule>(m_df_algs & 1, m_df_algs & 2,
                                             m_df_algs & 4, m_df_algs & 8,
                                             m_clock_frequency, m_link_idx,
                                             m_df_num_frames);

  // Fills the channel map at the beggining of a run
  auto chfiller = std::make_shared<ChannelMapFiller>("channelmapfiller", m_channel_map);

  // Initial tasks
  // Add some offset time to let the other parts of the DAQ start
  // Typically the first and maybe second requests of data fails
  if (m_standard_dqm_hist.how_often > 0)
    map[std::chrono::system_clock::now() + std::chrono::seconds(m_offset_from_channel_map)] = {
      hist,
      m_standard_dqm_hist.how_often,
      m_standard_dqm_hist.unavailable_time,
      m_standard_dqm_hist.num_frames,
      nullptr,
      "Histogram every " + std::to_string(m_standard_dqm_hist.how_often) + " s"
    };
  if (m_standard_dqm_mean_rms.how_often > 0)
    map[std::chrono::system_clock::now() + std::chrono::seconds(m_offset_from_channel_map)] = {
      mean_rms,
      m_standard_dqm_mean_rms.how_often,
      m_standard_dqm_mean_rms.unavailable_time,
      m_standard_dqm_mean_rms.num_frames,
      nullptr,
      "Mean and RMS every " + std::to_string(m_standard_dqm_mean_rms.how_often) + " s"
    };
  if (m_standard_dqm_fourier.how_often > 0)
    map[std::chrono::system_clock::now() + std::chrono::seconds(m_offset_from_channel_map)] = {
      fourier,
      m_standard_dqm_fourier.how_often,
      m_standard_dqm_fourier.unavailable_time,
      m_standard_dqm_fourier.num_frames,
      nullptr,
      "Fourier every " + std::to_string(m_standard_dqm_fourier.how_often) + " s"
    };

  if (m_standard_dqm_fourier_sum.how_often > 0)
    map[std::chrono::system_clock::now() + std::chrono::seconds(m_offset_from_channel_map)] = {
      fouriersum,
      m_standard_dqm_fourier_sum.how_often,
      m_standard_dqm_fourier_sum.unavailable_time,
      m_standard_dqm_fourier_sum.num_frames,
      nullptr,
      "Summed Fourier every " + std::to_string(m_standard_dqm_fourier_sum.how_often) + " s"
    };

  if (m_standard_channel_mask.how_often > 0)
    map[std::chrono::system_clock::now() + std::chrono::seconds(m_offset_from_channel_map)] = {
      channel_mask,
      m_standard_channel_mask.how_often,
      m_standard_channel_mask.unavailable_time,
      m_standard_channel_mask.num_frames,
      nullptr,
      "Channel Mask every " + std::to_string(m_standard_channel_mask.how_often) + " s"
    };

  if (m_mode == "df" && m_df_seconds > 0) {
    map[std::chrono::system_clock::now() + std::chrono::milliseconds(1000 * m_offset_from_channel_map + static_cast<int>(m_df_offset * 1000))] = {
      dfmodule,
      m_df_seconds,
      5,
      -1, // Number of frames, unused
      nullptr,
      "Algorithms on TRs from DF every " + std::to_string(m_df_seconds) + " s"
    };
  }


  map[std::chrono::system_clock::now() + std::chrono::seconds(m_channel_map_delay)] = { chfiller, 3,
                                                                      3,
                                                                      1, // Request only one frame for each link
                                                                      nullptr,  "Channel map filler" };

  // Main loop, running forever
  while (m_run_marker) {

    auto task = map.begin();
    if (task == map.end()) {
      throw ProcessorError(ERS_HERE, "Empty map! This should never happen!");
    }
    auto next_time = task->first;
    auto analysis_instance = task->second;
    auto algo = analysis_instance.mod;

    // Sleep until the next time, done in steps so that one doesn't have to wait a lot
    // when stopping
    while (m_run_marker && next_time - std::chrono::system_clock::now() > std::chrono::duration<double>(m_sleep_time / 1000.)) {
      std::this_thread::sleep_for(std::chrono::milliseconds(m_sleep_time));
    }
    if (!m_run_marker) break;
    std::this_thread::sleep_until(next_time);

    // Save pointer to delete the thread later
    auto previous_thread = analysis_instance.running_thread;

    // If the channel map filler has already run and has worked then remove the entry
    // and keep running
    if (analysis_instance.mod == chfiller && m_map->is_filled()) {
      // If the channel map filling has not finished yet
      // we wait until the it has finished and the thread is joined
      if (analysis_instance.running_thread != nullptr && analysis_instance.running_thread->joinable()) {
        analysis_instance.running_thread->join();
      }
      map.erase(task);
      TLOG_DEBUG(5) << "Channel map already filled, removing entry and starting again";
      continue;
    }

    // We don't want to run if the run has stopped after sleeping for a while
    if (!m_run_marker) {
      break;
    }

    // Make sure that the process is not running and a request can be made
    // otherwise we wait for more time
    if (algo->get_is_running()) {
      TLOG(5) << "ALGORITHM " << analysis_instance.name << " already running";
      map[std::chrono::system_clock::now() +
          std::chrono::milliseconds(static_cast<int>(analysis_instance.default_unavailable_time) * 1000)] = {
        algo,
        analysis_instance.between_time,
        analysis_instance.default_unavailable_time,
        analysis_instance.number_of_frames,
        previous_thread,
        analysis_instance.name
      };
      map.erase(task);
      continue;
    }

    // There was a bug where the timestamp was retrieved before there were any timestamps
    // obtaining an invalid timestamps
    if (m_mode == "readout") {
      auto timestamp = m_time_est->get_timestamp_estimate();
      if (timestamp == dfmessages::TypeDefaults::s_invalid_timestamp) {
        ers::warning(InvalidTimestamp(ERS_HERE, timestamp));
        // Some sleep is needed because at the beginning there are no valid timestamps
        // so it will be checking continuously if there is a valid one
        std::this_thread::sleep_for(std::chrono::milliseconds(m_timesync_check));
        continue;
      }
    }

    // Now it's the time to do something
    dfmessages::TriggerDecision request;
    if (m_mode == "readout") {
      request = CreateRequest(m_links, analysis_instance.number_of_frames);
      try {
        m_sink->push(request, m_sink_timeout);
      } catch (const ers::Issue& excpt) {
        TLOG() << "DQM: Unable to push to the request queue";
        continue;
      }
      TLOG_DEBUG(10) << "Request (trigger decision) pushed to the queue";
    }
    else if (m_mode == "df") {
      dfrequest();
    }

    ++m_request_count;
    ++m_total_request_count;

    if (m_mode == "readout") {
      try {
        m_source->pop(element, m_source_timeout);
      } catch (const ers::Issue& excpt) {
        TLOG() << "DQM: Unable to pop from the data queue";
        continue;
      }
      TLOG_DEBUG(10) << "Data popped from the queue";
    }
    else if (m_mode == "df") {
      while (m_run_marker && dftrs.get_num_elements() == 0) {
        std::this_thread::sleep_for(std::chrono::milliseconds(m_sleep_time_df));
      }
      if (!m_run_marker) {
        break;
      }
      dftrs.pop(element, std::chrono::milliseconds(100));
    }

    ++m_data_count;
    ++m_total_data_count;

    auto memfunc = &AnalysisModule::run;
    auto current_thread =
      std::make_shared<std::thread>(memfunc, std::ref(*algo), std::move(element), std::ref(m_run_marker), std::ref(m_map), std::ref(m_frontend_type), m_kafka_address);
    element.reset(nullptr);

    // Add a new entry for the current instance
    TLOG() << "Starting to run \"" << analysis_instance.name << "\"";
    map[std::chrono::system_clock::now() +
        std::chrono::milliseconds(static_cast<int>(analysis_instance.between_time) * 1000)] = {
      algo,
      analysis_instance.between_time,
      analysis_instance.default_unavailable_time,
      analysis_instance.number_of_frames,
      current_thread,
      analysis_instance.name
    };

    // Delete thread
    if (previous_thread != nullptr) {
      if (previous_thread->joinable()) {
        previous_thread->join();
      } else {
        throw ProcessorError(ERS_HERE, "Thread not joinable");
      }
    }

    map.erase(task);
  }

  for (auto& [time, analysis_instance] : map) {
    if (analysis_instance.running_thread && analysis_instance.running_thread->joinable()) {
      analysis_instance.running_thread->join();
    }
  }

  // Delete the timestamp estimator after we are sure we won't need it
  m_time_est.reset(nullptr);
} // NOLINT Function length

dfmessages::TriggerDecision
DQMProcessor::CreateRequest(std::vector<dfmessages::GeoID>& m_links, int number_of_frames)
{
  auto timestamp = m_time_est->get_timestamp_estimate();
  dfmessages::TriggerDecision decision;

  static daqdataformats::trigger_number_t trigger_number = 1;

  decision.trigger_number = trigger_number;
  trigger_number++;
  decision.run_number = m_run_number;
  decision.trigger_timestamp = timestamp;
  decision.readout_type = dfmessages::ReadoutType::kMonitoring;

  int window_size = number_of_frames * TICKS_BETWEEN_TIMESTAMP;

  for (auto& link : m_links) {
    // TLOG() << "ONE LINK";
    daqdataformats::ComponentRequest request;
    request.component = link;
    // Some offset is required to avoid having delayed requests in readout
    // which make the TRB to take longer and longer to create the trigger records
    // 10^5 was tried at first but wasn't enough. At 50 MHz, 10^5 = 2 ms
    // The current value is 10^7 which seems to work after several hours of
    // running and no delayed requests in readout
    request.window_begin = timestamp - window_size - m_readout_window_offset;
    request.window_end = timestamp - m_readout_window_offset;

    decision.components.push_back(request);
  }

  TLOG_DEBUG(10) << "Making request (trigger decision) asking for " << m_links.size()
                 << " links and with the beginning of the window at timestamp " << timestamp - window_size
                 << " and the end of the window at timestamp " << timestamp;
  ;

  return decision;
}


void
DQMProcessor::dispatch_trigger_record(ipm::Receiver::Response message)
{
  dftrs.push(std::move(serialization::deserialize<std::unique_ptr<daqdataformats::TriggerRecord>>(message.data)),
             std::chrono::milliseconds(100));
}


void
DQMProcessor::dfrequest()
{
  TLOG() << "Sending request to DF";
  dfmessages::TRMonRequest trmon;
  trmon.run_number = m_run_number;
  trmon.trigger_type = 1;
  trmon.data_destination = m_df2dqm_connection;

  auto trmon_message = serialization::serialize(trmon, serialization::kMsgPack);
  networkmanager::NetworkManager::get().send_to(m_dqm2df_connection, static_cast<const void*>(trmon_message.data()), trmon_message.size(), m_sink_timeout);
}


void
DQMProcessor::dispatch_timesync(ipm::Receiver::Response message)
{
  ++m_received_timesync_count;
  auto timesyncmsg = serialization::deserialize<dfmessages::TimeSync>(message.data);
  TLOG_DEBUG(13) << "Received TimeSync message with DAQ time= " << timesyncmsg.daq_time
                 << ", run=" << timesyncmsg.run_number << " (local run number is " << m_run_number << ")";
  if (m_time_est.get() != nullptr) {
    if (timesyncmsg.run_number == m_run_number) {
      m_time_est->add_timestamp_datapoint(timesyncmsg);
    } else {
      TLOG_DEBUG(0) << "Discarded TimeSync message from run " << timesyncmsg.run_number << " during run " << m_run_number;
    }
  }
}

} // namespace dqm
} // namespace dunedaq

// Define the module
DEFINE_DUNE_DAQ_MODULE(dunedaq::dqm::DQMProcessor)
