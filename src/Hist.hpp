#pragma once

/**
 * @file Hist.hpp Simple 1D histogram implementation
 *
 * This is part of the DUNE DAQ , copyright 2020.
 * Licensing/copyright details are in the COPYING file that you should have
 * received with this code.
 */

#include <vector>
#include <string>
#include <ostream>
#include <fstream>
#include <iostream>
#include <chrono>
#include <cstdlib>
#include <ctime>

// DQM
#include "AnalysisModule.hpp"
#include "Decoder.hpp"
#include "Exporter.hpp"

#include "dataformats/TriggerRecord.hpp"

// DQM
#include "AnalysisModule.hpp"
#include "Decoder.hpp"

#include "dataformats/TriggerRecord.hpp"

#include "PdspChannelMapService.cpp"

/**
 * Basic 1D histogram that counts entries
 * It only supports uniform binning
 * Overflow and underflow is not supported yet
 */
namespace dunedaq::dqm{

class Hist {

  int find_bin(double x) const;

public:
  double m_low, m_high, m_step_size;
  int m_nentries;
  double m_sum;
  
  int m_steps;
  std::vector<int> m_entries;

  Hist(int steps, double low, double high);
  int fill(double x);
  int scramble(double scrambulation);

  void save(const std::string &filename) const;
  void save(std::ofstream &filehandle) const;

  bool is_running();
  void run(dunedaq::dataformats::TriggerRecord &tr);
};



Hist::Hist(int steps, double low, double high)
  : m_low(low), m_high(high), m_steps(steps)
{
  m_entries = std::vector<int> (steps, 0);
  m_step_size = (high - low) / steps;
}

int
Hist::find_bin(double x) const
{
    return (x - m_low) / m_step_size;
}

int
Hist::fill(double x)
{
  int bin = find_bin(x);
  // Underflow, do nothing
  if(bin < 0) return -1;

  // Overflow, do nothing
  if(bin >= m_steps) return -1;

  m_entries[bin]++;
  m_nentries++;
  m_sum += x;
  return bin;
}

void
Hist::save(const std::string &filename) const
{
  std::ofstream file;
  file.open(filename);
  file << m_steps << " " << m_low << " " << m_high << " " << std::endl;
  for (auto x: m_entries)
    file << x << " ";
  file << std::endl;
}

void
Hist::save(std::ofstream &filehandle) const
{
  filehandle << m_steps << " " << m_low << " " << m_high << " " << std::endl;
  for (auto x: m_entries)
    filehandle << x << " ";
  filehandle << std::endl;
}

int
Hist::scramble(double scrambulation)
{
  //std::srand(time(NULL));

  for (int i = 0; i < m_steps; i++)
  {
    m_entries[i] +=  m_entries[i]*((((std::rand() % 20) - 10.)/10.)*scrambulation);
  }
  return 1;
}

bool
Hist::is_running()
{
  return true;
}

void
Hist::run(dunedaq::dataformats::TriggerRecord &tr)
{
  dunedaq::dqm::Decoder dec;
  auto wibframes = dec.decode(tr);

  for(auto &fr:wibframes){
    for(int ich=0; ich<256; ++ich)
      this->fill(fr.get_channel(ich));
  }
  this->save("Hist/hist.txt");
}

//=============================================================

class HistLink : public AnalysisModule{
  std::string m_name;
  std::vector<Hist> histvec;
  std::vector<std::string> chanvec;
  bool m_run_mark;

public:

  HistLink(std::string name, int steps, double low, double high);

  void run(dunedaq::dataformats::TriggerRecord &tr, std::string mode);
  void transmit(const std::string &topicname, int run_num, time_t timestamp, std::string chaninf) const;

  bool is_running();
};

HistLink::HistLink(std::string name, int steps, double low, double high)
  : m_name(name), m_run_mark(false)
{
  for(int i=0; i<256; ++i){
    std::string chaninf = "NULL NULL NULL";
    Hist hist(steps, low, high);
    histvec.push_back(hist);
    chanvec.push_back(chaninf);
  }
}

void HistLink::transmit(const std::string &topicname, int run_num, time_t timestamp, std::string chaninf) const
{
  std::stringstream csv_output;
  std::string datasource = "TESTSOURCE";
  std::string dataname   = this->m_name;
  std::string axislabel = "TESTLABEL";
  std::stringstream metadata;
  metadata << chaninf << " " << histvec[0].m_steps << " " << histvec[0].m_low << " " << histvec[0].m_high;

  int subrun = 0;
  int event = 0;
  
  //Construct CSV output
  csv_output << datasource << ";" << dataname << ";" << run_num << ";" << subrun << ";" << event << ";" << timestamp << ";" << metadata.str() << ";";
  csv_output << axislabel << "\n";
  for (int ich = 0; ich < 256; ++ich)
  {
    csv_output << chaninf << "\n";
    for (auto x: histvec[ich].m_entries) csv_output << x << " ";
    csv_output << "\n";
  }
  //csv_output << "\n"; 
  
  //Transmit
  KafkaExport(csv_output.str(), topicname);
}

  void HistLink::run(dunedaq::dataformats::TriggerRecord &tr, std::string mode){

  PdspChannelMapService channelMap("protoDUNETPCChannelMap_RCE_v4.txt", "protoDUNETPCChannelMap_FELIX_v4.txt");
  std::srand((unsigned) time(NULL));

  m_run_mark = true;
  dunedaq::dqm::Decoder dec;
  auto wibframes = dec.decode(tr);

  for(auto &fr:wibframes){

    for(int ich=0; ich<256; ++ich)
    {
      //Get channel info
      int crate = fr.get_wib_header()->crate_no;
      int slot = fr.get_wib_header()->slot_no;
      int fiber = fr.get_wib_header()->fiber_no;
      unsigned int fiberloc = FiberLoc(fiber);
      unsigned int crateloc = crate;
      unsigned int chloc = ich;
      if (chloc > 127)
      {
        chloc -= 128;
        fiberloc++;
      }
      unsigned int offline = channelMap.GetOfflineNumberFromDetectorElements(crateloc, slot, fiberloc, chloc, PdspChannelMapService::kFELIX);
      //unsigned int offline = ich;
      unsigned int plane = channelMap.PlaneFromOfflineChannel(offline);
      unsigned int apa = channelMap.APAFromOfflineChannel(offline);
      std::stringstream chan_info;
      //chan_info << apa << " " << plane << " " << offline;
      chan_info << offline;
      
      histvec[ich].fill(fr.get_channel(ich));
      chanvec[ich] = chan_info.str();
    }
  }

  //Add random variation to histograms 
  for (int ich=0; ich<256; ++ich)
  {
    int scramblor = histvec[ich].scramble(.25);
  }

  //Transmit via kafka
  this->transmit("testdunedqm", tr.get_header_ref().get_run_number(), tr.get_header_ref().get_trigger_timestamp(), chanvec[0]);

  for(int ich=0; ich<256; ++ich)
  {
    histvec[ich].save("Hist/" + m_name + "-" + std::to_string(ich) + ".txt");
  }
  m_run_mark = false;
}

bool HistLink::is_running(){
  return m_run_mark;
}
 

} // namespace dunedaq::dqm
