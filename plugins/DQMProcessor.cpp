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

#include "DQMProcessor.hpp"

// Channel map and other utilities
#include "dqm/Constants.hpp"
#include "dqm/DQMLogging.hpp"
#include "dqm/ChannelMap.hpp"
#include "dqm/ChannelMapFiller.hpp"

// DUNE-DAQ includes
#include "daqdataformats/ComponentRequest.hpp"
#include "daqdataformats/SourceID.hpp"
#include "dfmessages/TimeSync.hpp"
#include "dfmessages/TRMonRequest.hpp"
#include "dfmessages/TriggerRecord_serialization.hpp"

// C++ includes
#include <chrono>
#include <map>
#include <memory>
#include <string>
#include <thread>
#include <vector>

#include "dqm/modules/DFModule.hpp"
#ifndef WITH_PYTHON_SUPPORT
// Modules with the classes that contain the algorithms
#include "dqm/modules/CounterModule.hpp"
#include "dqm/modules/STDModule.hpp"
#include "dqm/modules/RMSModule.hpp"
#include "dqm/modules/FourierContainer.hpp"
#else
#include "dqm/modules/Python.hpp"
#include "dqm/PythonUtils.hpp"
#endif

namespace dunedaq {
namespace dqm {

using logging::TLVL_WORK_STEPS;
using logging::TLVL_DATA_SENT_OR_RECEIVED;

DQMProcessor::DQMProcessor(const std::string& name)
  : DAQModule(name)
{
  register_command("start", &DQMProcessor::do_start);
  register_command("conf", &DQMProcessor::do_configure);
  register_command("drain_dataflow", &DQMProcessor::do_drain_dataflow);
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

  fcr.raw_times_run = m_dqm_info.raw_times_run.exchange(0);
  fcr.raw_time_taken = m_dqm_info.raw_time_taken.load();

  fcr.std_times_run = m_dqm_info.std_times_run.exchange(0);
  fcr.std_time_taken = m_dqm_info.std_time_taken.load();

  fcr.rms_times_run = m_dqm_info.rms_times_run.exchange(0);
  fcr.rms_time_taken = m_dqm_info.rms_time_taken.load();

  fcr.fourier_channel_times_run = m_dqm_info.fourier_channel_times_run.exchange(0);
  fcr.fourier_channel_time_taken = m_dqm_info.fourier_channel_time_taken.load();

  fcr.fourier_plane_times_run = m_dqm_info.fourier_plane_times_run.exchange(0);
  fcr.fourier_plane_time_taken = m_dqm_info.fourier_plane_time_taken.load();

  fcr.channel_map_total_channels = m_dqm_info.channel_map_total_channels.load();
  fcr.channel_map_total_planes = m_dqm_info.channel_map_total_planes.load();

  ci.add(fcr);
}

void
DQMProcessor::do_configure(const nlohmann::json& args)
{
  auto conf = args.get<dqmprocessor::Conf>();
  m_kafka_address = conf.kafka_address;
  m_kafka_topic = conf.kafka_topic;

  m_mode = conf.mode;
  m_frontend_type = conf.frontend_type;

  m_raw_conf = conf.raw;
  m_rms_conf = conf.rms;
  m_std_conf = conf.std;
  m_fourier_channel_conf = conf.fourier_channel;
  m_fourier_plane_conf = conf.fourier_plane;

  m_df_seconds = conf.df_seconds;
  m_df_offset = conf.df_offset;
  m_df_algs = conf.df_algs;
  m_df_num_frames = conf.df_num_frames;

  m_link_idx = conf.link_idx;
  m_clock_frequency = conf.clock_frequency;
  m_channel_map = conf.channel_map;
  m_readout_window_offset = conf.readout_window_offset;

  m_df2dqm_connection = conf.df2dqm_connection_name;
  m_dqm2df_connection = conf.dqm2df_connection_name;

  m_max_frames = conf.max_num_frames;

  m_dqm_args = DQMArgs{m_run_marker, std::shared_ptr<ChannelMap>(new ChannelMap),
                       m_frontend_type, m_kafka_address,
                       m_kafka_topic, m_max_frames};

  if (m_mode == "readout") {
    m_receiver = get_iom_receiver<std::unique_ptr<daqdataformats::TriggerRecord>>("trigger_record_q_dqm");

    std::ostringstream conn_uid_oss;
    conn_uid_oss << "trigger_decision_" << conf.trb_sourceid;
    m_sender = get_iom_sender<dfmessages::TriggerDecision>(conn_uid_oss.str());
  }
}

void
DQMProcessor::do_start(const nlohmann::json& args)
{
  if (m_mode == "readout") {

    m_time_est.reset(new timinglibs::TimestampEstimator(m_clock_frequency));

    m_received_timesync_count.store(0);

    // Subscribe to all TimeSync messages
    get_iomanager()->add_callback<dfmessages::TimeSync>(
      ".*", std::bind(&DQMProcessor::dispatch_timesync, this, std::placeholders::_1));

  }

  if (m_mode == "df") {
    get_iomanager()->add_callback<std::unique_ptr<daqdataformats::TriggerRecord>>(
      m_df2dqm_connection, std::bind(&DQMProcessor::dispatch_trigger_record, this, std::placeholders::_1));

  }

  m_dqm_args.run_mark = std::make_shared<std::atomic<bool>>(true);

  m_run_number.store(daqdataformats::run_number_t(args.at("run").get<daqdataformats::run_number_t>()));

  m_running_thread.reset(new std::thread(&DQMProcessor::do_work, this));
}

void
DQMProcessor::do_drain_dataflow(const data_t&)
{
  m_dqm_args.run_mark->store(false);
  m_running_thread->join();

  if (m_mode == "readout") {
    get_iomanager()->remove_callback<dfmessages::TimeSync>(".*");
  }
  else if (m_mode == "df") {
    get_iomanager()->remove_callback<std::unique_ptr<daqdataformats::TriggerRecord>>(m_df2dqm_connection);
  }
  TLOG() << get_name() << ": received " << m_received_timesync_count.load() << " TimeSync messages.";
}

void
DQMProcessor::do_work()
{

  // Helper struct with the necessary information about an instance
  struct AnalysisInstance
  {
    std::shared_ptr<AnalysisModule> mod;
    int between_time;
    int number_of_frames;
    std::shared_ptr<std::thread> running_thread;
    std::string name;
  };

  std::vector<daqdataformats::SourceID> m_sids;

  for (auto i : m_link_idx) {
    m_sids.push_back({ daqdataformats::SourceID::Subsystem::kDetectorReadout, static_cast<unsigned int>(i) });
  }
  std::sort(m_sids.begin(), m_sids.end());

  // Map that holds the tasks and times when to do them
  std::map<std::chrono::time_point<std::chrono::system_clock>, AnalysisInstance> map;

  std::unique_ptr<daqdataformats::TriggerRecord> element{ nullptr };

  // Instances of analysis modules

  // Fills the channel map at the beggining of a run
  auto chfiller = std::make_shared<ChannelMapFiller>("channelmapfiller", m_channel_map);
  TLOG() << "m_df_algs = " << m_df_algs;
  auto dfmodule = std::make_shared<DFModule>(m_df_algs.find("raw") != std::string::npos,
                                             m_df_algs.find("rms") != std::string::npos,
                                             m_df_algs.find("std") != std::string::npos,
                                             m_df_algs.find("fourier_channel") != std::string::npos,
                                             m_df_algs.find("fourier_plane") != std::string::npos,
                                             m_clock_frequency, m_link_idx,
                                             m_df_num_frames, m_frontend_type);

#ifndef WITH_PYTHON_SUPPORT
  // Raw event display
  auto raw = std::make_shared<CounterModule>(
        "raw", CHANNELS_PER_LINK * m_link_idx.size(), m_link_idx);
  // STD
  auto std = std::make_shared<STDModule>("std", CHANNELS_PER_LINK * m_link_idx.size(), m_link_idx);
  // RMS
  auto rms = std::make_shared<RMSModule>("rms", CHANNELS_PER_LINK * m_link_idx.size(), m_link_idx);
  // Fourier transform
  // The Delta of time between frames is the inverse of the sampling frequency (clock frequency)
  // but because we are sampling every TICKS_BETWEEN_TIMESTAMP ticks we have to multiply by that
  auto fourier_channel = std::make_shared<FourierContainer>("fourier_channel",
                                                      CHANNELS_PER_LINK * m_link_idx.size(),
                                                      m_link_idx,
                                                            1. / m_clock_frequency * ((m_frontend_type == "wib") ? 25 : 32),
                                                      m_fourier_channel_conf.num_frames);
  auto fourier_plane = std::make_shared<FourierContainer>("fourier_plane",
                                                      4,
                                                      m_link_idx,
                                                       1. / m_clock_frequency * ((m_frontend_type == "wib") ? 25 : 32),
                                                      m_fourier_plane_conf.num_frames,
                                                      true);



  // Initial tasks
  // Add some offset time to let the other parts of the DAQ start
  // Typically the first and maybe second requests of data fails
  if (m_raw_conf.how_often > 0)
    map[std::chrono::system_clock::now() + std::chrono::seconds(m_offset_from_channel_map)] = {
      raw,
      m_raw_conf.how_often,
      m_raw_conf.num_frames,
      nullptr,
      "Raw data every " + std::to_string(m_raw_conf.how_often) + " s"
    };
  if (m_std_conf.how_often > 0)
    map[std::chrono::system_clock::now() + std::chrono::seconds(m_offset_from_channel_map)] = {
      std,
      m_std_conf.how_often,
      m_std_conf.num_frames,
      nullptr,
      "STD every " + std::to_string(m_std_conf.how_often) + " s"
    };
  if (m_rms_conf.how_often > 0)
    map[std::chrono::system_clock::now() + std::chrono::seconds(m_offset_from_channel_map)] = {
      rms,
      m_rms_conf.how_often,
      m_rms_conf.num_frames,
      nullptr,
      "RMS every " + std::to_string(m_rms_conf.how_often) + " s"
    };
  if (m_fourier_channel_conf.how_often > 0)
    map[std::chrono::system_clock::now() + std::chrono::seconds(m_offset_from_channel_map)] = {
      fourier_channel,
      m_fourier_channel_conf.how_often,
      m_fourier_channel_conf.num_frames,
      nullptr,
      "Fourier (for every channel) every " + std::to_string(m_fourier_channel_conf.how_often) + " s"
    };

  if (m_fourier_plane_conf.how_often > 0)
    map[std::chrono::system_clock::now() + std::chrono::seconds(m_offset_from_channel_map)] = {
      fourier_plane,
      m_fourier_plane_conf.how_often,
      m_fourier_plane_conf.num_frames,
      nullptr,
      "Fourier (for every plane) every " + std::to_string(m_fourier_plane_conf.how_often) + " s"
    };

  if (m_mode == "df" && m_df_seconds > 0) {
    map[std::chrono::system_clock::now() + std::chrono::milliseconds(1000 * m_offset_from_channel_map + static_cast<int>(m_df_offset * 1000))] = {
      dfmodule,
      m_df_seconds,
      -1, // Number of frames, unused
      nullptr,
      "Algorithms on TRs from DF every " + std::to_string(m_df_seconds) + " s"
    };
  }

#else
  Py_Initialize();
  np::initialize();


typedef std::map<int, std::vector<detdataformats::wib::WIBFrame*>> mapt;
  p::class_<std::map<int, std::vector<detdataformats::wib::WIBFrame*>>>("MapWithFrames")
  .def("__len__", &mapt::size)
  .def("__getitem__", &MapItem<mapt>::get
      // return_value_policy<copy_non_const_reference>()
       )
  .def("get_adc", &MapItem<mapt>::get_adc
      // return_value_policy<copy_non_const_reference>()
       )
    ;

  p::class_<std::vector<np::ndarray>>("VecClass")
  .def("__len__", &std::vector<np::ndarray>::size)
  .def("__getitem__", &std_item<std::vector<np::ndarray>>::get,
       p::return_value_policy<p::copy_non_const_reference>()
         )
  ;

  auto std_python = std::make_shared<PythonModule>("std");
  if (m_std_conf.how_often > 0)
    map[std::chrono::system_clock::now() + std::chrono::seconds(m_offset_from_channel_map)] = {
      std_python,
      m_std_conf.how_often,
      m_std_conf.num_frames,
      nullptr,
      "STD every " + std::to_string(m_std_conf.how_often) + " s"
    };

  auto rms_python = std::make_shared<PythonModule>("rms");
  if (m_rms_conf.how_often > 0)
    map[std::chrono::system_clock::now() + std::chrono::seconds(m_offset_from_channel_map)] = {
      rms_python,
      m_rms_conf.how_often,
      m_rms_conf.num_frames,
      nullptr,
      "RMS every " + std::to_string(m_rms_conf.how_often) + " s"
    };

  auto raw_python = std::make_shared<PythonModule>("raw");
  if (m_raw_conf.how_often > 0)
    map[std::chrono::system_clock::now() + std::chrono::seconds(m_offset_from_channel_map + 1)] = {
      raw_python,
      m_raw_conf.how_often,
      m_raw_conf.num_frames,
      nullptr,
      "Raw every " + std::to_string(m_raw_conf.how_often) + " s"
    };

  auto fourier_plane_python = std::make_shared<PythonModule>("fp");
  if (m_fourier_plane_conf.how_often > 0)
    map[std::chrono::system_clock::now() + std::chrono::seconds(m_offset_from_channel_map + 1)] = {
      fourier_plane_python,
      m_fourier_plane_conf.how_often,
      m_fourier_plane_conf.num_frames,
      nullptr,
      "Fourier plane every " + std::to_string(m_fourier_plane_conf.how_often) + " s"
    };

  if (m_mode == "df" && m_df_seconds > 0) {
    map[std::chrono::system_clock::now() + std::chrono::milliseconds(1000 * m_offset_from_channel_map + static_cast<int>(m_df_offset * 1000))] = {
      dfmodule,
      m_df_seconds,
      -1, // Number of frames, unused
      nullptr,
      "Algorithms on TRs from DF every " + std::to_string(m_df_seconds) + " s"
    };
  }

  PyThreadState *_save;
  _save = PyEval_SaveThread();
#endif


  map[std::chrono::system_clock::now() + std::chrono::seconds(m_channel_map_delay)] = { chfiller,
                                                                                        3,
                                                                                        1, // Request only one frame for each link
                                                                                        nullptr,  "Channel map filler" };

  // Main loop, running forever
  while (*m_dqm_args.run_mark) {

    auto task = map.begin();
    if (task == map.end()) {
      throw ProcessorError(ERS_HERE, "Empty map! This should never happen!");
    }
    auto next_time = task->first;
    auto analysis_instance = task->second;
    auto algo = analysis_instance.mod;

    // Sleep until the next time, done in steps so that one doesn't have to wait a lot
    // when stopping
    while (*m_dqm_args.run_mark && next_time - std::chrono::system_clock::now() > std::chrono::duration<double>(m_sleep_time / 1000.)) {
      std::this_thread::sleep_for(std::chrono::milliseconds(m_sleep_time));
    }
    if (!*m_dqm_args.run_mark) break;
    std::this_thread::sleep_until(next_time);

    // Save pointer to delete the thread later
    auto previous_thread = analysis_instance.running_thread;

    // If the channel map filler has already run and has worked then remove the entry
    // and keep running
    if (analysis_instance.mod == chfiller && m_dqm_args.map->is_filled()) {
      // If the channel map filling has not finished yet
      // we wait until the it has finished and then thread is joined
      if (analysis_instance.running_thread != nullptr && analysis_instance.running_thread->joinable()) {
        analysis_instance.running_thread->join();
      }
      map.erase(task);
      TLOG_DEBUG(5) << "Channel map already filled, removing entry and starting again";
      continue;
    }
    else if (analysis_instance.mod != chfiller && !m_dqm_args.map->is_filled()) {
      map[std::chrono::system_clock::now() +
          // We wait 10% of the time between runs of the algorithm
          std::chrono::milliseconds(static_cast<int>(analysis_instance.between_time * 100.0))] = {
        algo,
        analysis_instance.between_time,
        analysis_instance.number_of_frames,
        previous_thread,
        analysis_instance.name
      };
      map.erase(task);
      continue;
    }

    // We don't want to run if the run has stopped after sleeping for a while
    if (!*m_dqm_args.run_mark) {
      break;
    }

    // Make sure that the process is not running and a request can be made
    // otherwise we wait for more time
    if (algo->get_is_running()) {
      TLOG(5) << "ALGORITHM " << analysis_instance.name << " already running";
      map[std::chrono::system_clock::now() +
          // We wait 10% of the time between runs of the algorithm
          std::chrono::milliseconds(static_cast<int>(analysis_instance.between_time * 100.0))] = {
        algo,
        analysis_instance.between_time,
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
      request = create_readout_request(m_sids, analysis_instance.number_of_frames, m_dqm_args.frontend_type);
      try {
        m_sender->send(std::move(request), m_sink_timeout);
      } catch (iomanager::TimeoutExpired&) {
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
        element = m_receiver->receive(m_source_timeout);
      } catch (const ers::Issue& excpt) {
        TLOG() << "DQM: Unable to pop from the data queue";
        continue;
      }
      TLOG_DEBUG(TLVL_DATA_SENT_OR_RECEIVED) << "Data received from readout";
    }
    else if (m_mode == "df") {
      while (*m_dqm_args.run_mark && dftrs.get_num_elements() == 0) {
        std::this_thread::sleep_for(std::chrono::milliseconds(m_sleep_time_df));
      }
      if (!*m_dqm_args.run_mark) {
        break;
      }
      dftrs.pop(element, std::chrono::milliseconds(100));
      TLOG_DEBUG(TLVL_DATA_SENT_OR_RECEIVED) << "Data received from DF";
    }

    ++m_data_count;
    ++m_total_data_count;

    std::shared_ptr<daqdataformats::TriggerRecord> record = std::move(element);
    auto memfunc = &AnalysisModule::run;
    auto current_thread =
      std::make_shared<std::thread>(memfunc, std::ref(*algo), record, std::ref(m_dqm_args), std::ref(m_dqm_info));
    element.reset(nullptr);

    // Add a new entry for the current instance
    TLOG() << "Running \"" << analysis_instance.name << "\"";
    map[std::chrono::system_clock::now() +
        std::chrono::milliseconds(static_cast<int>(analysis_instance.between_time) * 1000)] = {
      algo,
      analysis_instance.between_time,
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

#ifdef WITH_PYTHON_SUPPORT
  PyEval_RestoreThread(_save);
#endif

  // Delete the timestamp estimator after we are sure we won't need it
  m_time_est.reset(nullptr);
} // NOLINT Function length

dfmessages::TriggerDecision
DQMProcessor::create_readout_request(std::vector<dfmessages::SourceID>& m_sids, int number_of_frames, std::string& frontend_type)
{
  auto timestamp = m_time_est->get_timestamp_estimate();
  dfmessages::TriggerDecision decision;

  static daqdataformats::trigger_number_t trigger_number = 1;

  decision.trigger_number = trigger_number;
  trigger_number++;
  decision.run_number = m_run_number;
  decision.trigger_timestamp = timestamp;
  decision.readout_type = dfmessages::ReadoutType::kMonitoring;

  int window_size = number_of_frames * get_ticks_between_timestamps(frontend_type);

  for (auto& sid : m_sids) {
    daqdataformats::ComponentRequest request;
    request.component = sid;
    // Some offset is required to avoid having delayed requests in readout
    // which make the TRB to take longer and longer to create the trigger records
    // 10^5 was tried at first but wasn't enough. At 50 MHz, 10^5 = 2 ms
    // The current value is 10^7 which seems to work after several hours of
    // running and no delayed requests in readout
    request.window_begin = timestamp - window_size - m_readout_window_offset;
    request.window_end = timestamp - m_readout_window_offset;

    decision.components.push_back(request);
  }

  TLOG_DEBUG(10) << "Making request (trigger decision) asking for " << m_sids.size()
                 << " links and with the beginning of the window at timestamp " << timestamp - window_size
                 << " and the end of the window at timestamp " << timestamp;
  ;

  return decision;
}


void
DQMProcessor::dispatch_trigger_record(std::unique_ptr<daqdataformats::TriggerRecord>& tr)
{
  dftrs.push(std::move(tr), std::chrono::milliseconds(100));
}


void
DQMProcessor::dfrequest()
{
  TLOG() << "Sending request to DF";
  dfmessages::TRMonRequest trmon;
  trmon.run_number = m_run_number;
  trmon.trigger_type = 1;
  trmon.data_destination = m_df2dqm_connection;

  get_iom_sender<dfmessages::TRMonRequest>(m_dqm2df_connection)->send(std::move(trmon), m_sink_timeout);
}


void
DQMProcessor::dispatch_timesync(dfmessages::TimeSync& timesyncmsg)
{
  ++m_received_timesync_count;
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
