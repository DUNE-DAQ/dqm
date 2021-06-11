/**
 * @file DataReceiver.cpp DataReceiver Class Implementation
 *
 * See header for more on this class
 *
 * This is part of the DUNE DAQ Application Framework, copyright 2020.
 * Licensing/copyright details are in the COPYING file that you should have
 * received with this code.
 */

#include <chrono>
#include <thread>

// DQM includes
#include "DataReceiver.hpp"
#include "Hist.hpp"
#include "Fourier.hpp"

#include "appfwk/DAQSource.hpp"

#include "readout/ReadoutTypes.hpp"

#include "dfmessages/TriggerDecision.hpp"

#include "dataformats/Fragment.hpp"
#include "dataformats/TriggerRecord.hpp"
#include "dataformats/GeoID.hpp"
#include "dataformats/ComponentRequest.hpp"
#include "dataformats/wib/WIBFrame.hpp"

#include "dqm/datareceiver/Nljs.hpp"
#include "dqm/datareceiver/Structs.hpp"
namespace dunedaq{
namespace dqm {

int MAX_HIST_FRAME = 9;
int rolling_index = 10;

DataReceiver::DataReceiver(const std::string& name) : DAQModule(name)
{
  register_command("start", &DataReceiver::do_start);
  register_command("conf", &DataReceiver::do_configure);
  register_command("stop", &DataReceiver::do_stop);
}

void
DataReceiver::init(const data_t&)
{
  m_source.reset(new trigger_record_source_qt("trigger_record_q_dqm"));
  m_sink.reset(new trigger_decision_sink_qt("trigger_decision_q_dqm"));
}

void
DataReceiver::do_configure(const nlohmann::json& args)
{
  auto conf = args.get<datareceiver::Conf>();
  m_running_mode = conf.mode;
  // m_source = std::unique_ptr<appfwk::DAQSource < std::unique_ptr<dataformats::TriggerRecord >>> ("trigger_record_q_dqm");
  // m_sink = std::unique_ptr<appfwk::DAQSink < dfmessages::TriggerDecision >> ("trigger_decision_q_dqm");
}

void
DataReceiver::do_start(const data_t&)
{
  m_run_marker.store(true);
  RequestMaker();
}

void
DataReceiver::do_stop(const data_t&)
{
  m_run_marker.store(false);
}

void decode(dataformats::TriggerRecord &record){
  std::vector<std::unique_ptr<dataformats::Fragment>>& fragments = record.get_fragments_ref();
  // std::cout << "Size of fragments is " << fragments.size() << std::endl;
  // for (auto &p: fragments) {
  //   TLOG() << "Fragment: " << p->get_trigger_number() << " " << p->get_size();
  //   auto datap = p->get_size();
  // }
  std::ofstream file("Hist/hist.txt");

  rolling_index++;
  if(rolling_index >= 20)
    rolling_index -= 10;
  //TLOG() << "rolling_indx" << rolling_index;


  std::vector<Hist> v;
  for(int i=0; i<256; ++i)
      v.push_back(Hist(100, 0, 5000));

  auto &fr = fragments[0];
  auto data = fr->get_data();
  std::vector<std::pair<void*, size_t>> frag_pieces;
  size_t offset = sizeof(dataformats::FragmentHeader);
  void *data_array = malloc(5568);

  for(int iframe=0; iframe<20; ++iframe){
    if(offset + 5568 >= fr->get_size()){
      TLOG() << "EXITING!!!!!!!!!!!!" << fr->get_size();
      break;
    }
    memcpy(data_array, (char*)data + offset, 5568);
    auto pair = std::make_pair<void*, size_t> (static_cast<void*>(data_array), (size_t)5568);
    frag_pieces.push_back(pair);
    offset += 5568;
    // TLOG() << "i = " << i;

    dataformats::WIBFrame *frame = static_cast<dataformats::WIBFrame*> (frag_pieces[iframe].first);

    for(int i=0; i<256; ++i) {
        // TLOG() << "Channel i: " << frame->get_channel(i);
        // TLOG() << "Channel i: " << frame->get_channel(i);
        v[i].fill(frame->get_channel(i));
    }
    if (iframe == rolling_index) {
      for(int i=0; i<256; ++i)
        v[i].save(file);
      file.close();
      return;
    }
  }

}

void DataReceiver::RequestMaker(){
  // Helper struct with the necessary information
  struct AnalysisInstance{
    AnalysisModule *mod;
    double between_time;
    double default_unavailable_time;
  };

  // Frequencies (how many seconds do we wait until running the next one?
  // Hard coded, should be read from another place
  double histogram_time = 1;
  int histogram_frames = 10;
  int histogram_default_unavailable = 1;
  double fourier_time = 10;
  int fourier_frequency = 10;
  int fourier_default_unavailable = 5;
  std::vector<double> times {histogram_time, fourier_time};

  // For now only one link
  std::vector<dfmessages::GeoID> m_links;
  m_links.push_back({dataformats::GeoID::SystemType::kTPC,  0, 0 });

  std::map<std::chrono::time_point<std::chrono::system_clock>, AnalysisInstance> map;

  std::unique_ptr<dataformats::TriggerRecord> element;

  // Instances of analysis modules
  HistLink hist1s("hist1s", 100, 0, 5000);
  //HistLink hist5s("hist5s", 50, 0, 5000);
  //HistLink hist10s("hist10s", 10, 0, 5000);
  FourierLink fourier10s("fourier10s", 0, 2500, 100);

  // Initial tasks
  //map[std::chrono::system_clock::now()] = {&hist1s, 1, 1};
  //map[std::chrono::system_clock::now()] = {&hist5s, 5, 1};
  //map[std::chrono::system_clock::now()] = {&hist10s, 10, 1};
  map[std::chrono::system_clock::now()] = {&fourier10s, 10, 2};

  std::vector<std::thread> threadvec;
  threadvec.reserve(4);

  // Main loop, running forever
  while(m_run_marker){
    auto fr = map.begin();
    if (fr == map.end()){
      TLOG() << "Empty map! This should never happen!";
      break;
    }
    auto next_time = fr->first;
    AnalysisModule *algo = fr->second.mod;

    //TLOG() << "TIME: next_time" << next_time.time_since_epoch().count();

    //TLOG() << "MAIN LOOP" << " Instance of " << fr->second.between_time << " seconds" << (algo == &hist1s) << " " << (algo == &hist5s);
    
    // Sleep until the next time
    std::this_thread::sleep_until(next_time);

    // Make sure that the process is not running and a request can be made
    // otherwise we wait for more time
    if(algo->is_running()) {
      TLOG() << "ALGORITHM already running"; 
      map[std::chrono::system_clock::now()+std::chrono::milliseconds((int)fr->second.default_unavailable_time*1000)] = {algo, fr->second.between_time, fr->second.default_unavailable_time};
      map.erase(fr);
      continue;
    }
      
    // Now it's the time to do something
    auto request = CreateRequest(m_links);
    m_sink->push(request, m_sink_timeout);

    //TLOG() << "Request pushed";

    //TLOG() << "Going to pop";
    m_source->pop(element, m_source_timeout);
    //TLOG() << "Element popped";
    //std::thread *t = new std::thread(&AnalysisModule::run, std::ref(*algo), std::ref(*element));
    threadvec.emplace_back(std::thread(&AnalysisModule::run, std::ref(*algo), std::ref(*element), m_running_mode));
    for (auto &t: threadvec) t.join();
    

    //Add a new entry for the current instance
    map[std::chrono::system_clock::now()+std::chrono::milliseconds((int)fr->second.between_time*1000)] = {algo, fr->second.between_time, fr->second.default_unavailable_time};

    // Delete the entry we just used and find the next one
    map.erase(fr);
  }

}

  dfmessages::TriggerDecision DataReceiver::CreateRequest(std::vector<dfmessages::GeoID> m_links){

    dfmessages::TriggerDecision decision;
    // decision.components.clear();
    //TLOG() << "decision.components.size() after clear: " << decision.components.size();
    decision.trigger_number = 1;

    decision.trigger_timestamp = 1;
    // decision.components = std::vector<dataformats::ComponentRequest>();
    // decision.components.reserve(2);

    std::vector<dfmessages::ComponentRequest> v;
    // decision.components.push_back()));

    for (auto &link : m_links) 
    {
      //TLOG() << "ONE LINK";
      dataformats::ComponentRequest request;
      request.component = link;
      request.window_end = 1000;
      request.window_begin = 0;
      // request.window_begin = timestamp - m_trigger_window_offset;
      // request.window_end = request.window_begin + window_ticks_dist(random_engine);
      // dfmessages::ComponentRequest request(link, 0, 1000);
        
      //TLOG() << "decision.components.size() before push: " << decision.components.size();
      v.push_back(request);
      decision.components.push_back(request);
      //TLOG() << "decision.components.size() after push: " << decision.components.size();
    }
    //TLOG() << "decision.components.size() before push: " << decision.components.size();
    //TLOG() << "decision.components.size() after push: " << decision.components.size();

    // TLOG() << "m_links.size(): " << m_links.size();
    // decision.components.push_back( dfmessages::ComponentRequest(m_links[0], 0, 1000));
    //TLOG() << "decision.components.size(): " << decision.components.size();
    return decision;
  }

} // namespace dunedaq
} // namespace dqm

// Define the module
DEFINE_DUNE_DAQ_MODULE(dunedaq::dqm::DataReceiver)
