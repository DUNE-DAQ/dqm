#pragma once

/**
 * @file Fourier.hpp Fast fourier transform using the Cooley-Tukey algorithm
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
#include <complex>
#include <valarray>

// dqm
//#include "AnalysisModule.hpp"
//#include "Decoder.hpp"

#include "dataformats/TriggerRecord.hpp"

#include "Exporter.hpp"
#include "PdspChannelMapService.cpp"

namespace dunedaq::dqm{
 
typedef std::complex<double> Complex;
typedef std::valarray<Complex> CArray;

class Fourier {

  double find_value(uint64_t time);
  CArray fourier_prep(const std::vector<double> &input) const;
  CArray fourier_rebin(CArray input, double factor);
  void fast_fourier_transform_1(CArray &x);
  void fast_fourier_transform_2(CArray &x);

public:
  uint64_t m_start, m_end, m_inc_size;
  int m_npoints;
  std::vector<double> m_data;
  double m_rebin_factor, m_duration;
  //CArray m_fourier_transform (10000);
  std::vector<double> m_fourier_transform;

  //Fourier(uint64_t start, uint64_t end, int npoints);
  Fourier(double duration, int npoints);
  int enter(double value, uint64_t time);
  void compute_fourier(double rebin_factor);

  void save(const std::string &filename) const;
  void save(std::ofstream &filehandle) const;

  void save_fourier(const std::string &filename) const;
  void save_fourier(std::ofstream &filehandle) const;
};


//Fourier::Fourier(uint64_t start, uint64_t end, int npoints) 
//  : m_start(start), m_end(end), m_npoints(npoints)
Fourier::Fourier(double duration, int npoints)
{
  //m_start = start;
  //m_end = end;
  //m_npoints = npoints;
  m_duration = duration;
  m_npoints = npoints;
  m_start = -1;
  m_end = -1;

  //uint64_t npoints64 = (uint64_t) npoints;
  //m_inc_size = (end - start)/npoints64;
  m_inc_size = 25;
  m_data = std::vector<double> (npoints, 0);
}

CArray Fourier::fourier_prep(const std::vector<double> &input) const
{
  std::valarray<Complex> output (input.size());
  Complex val;
 
  //Compute mean in order to centre time series around 0 in y 
  double mean = 0;
  for (size_t i = 0; i < input.size(); i++)
  {
    mean += input[i];
  }
  mean = (double) mean/(input.size());

  //Return shifted time series
  for (size_t i = 0; i < input.size(); i++)
  {
    val = input[i] - mean;
    output[i] = val;
  }
  return output;
}

CArray Fourier::fourier_rebin(CArray input, double factor)
{
  int newsize = (int) (input.size()/factor);
  std::valarray<Complex> output (newsize);
  for (size_t i = 0; i < input.size(); i++)
  {
    int k = (int) i/factor;
    //std::cout << "i = " << i << ", k = " << k << std::endl;
    output[k] += input[i];
  }
  return output;
}

//Cooley–Tukey FFT (in-place, divide-and-conquer)
// Higher memory requirements and redundancy although more intuitive
void Fourier::fast_fourier_transform_1(CArray& x)
{ 
  const size_t N = x.size();
  if (N <= 1) return;
           
  // divide
  CArray even = x[std::slice(0, N/2, 2)];
  CArray  odd = x[std::slice(1, N/2, 2)];
  
  // conquer
  fast_fourier_transform_1(even);
  fast_fourier_transform_1(odd);
  
  // combine
  for (size_t k = 0; k < N/2; ++k)
  { 
    Complex t = std::polar(1.0, -2 * 3.14159265358979323846264338328 * k / N) * odd[k];
    x[k    ] = even[k] + t;
    x[k+N/2] = even[k] - t;
  }
}


// Cooley-Tukey FFT (in-place, breadth-first, decimation-in-frequency)
void Fourier::fast_fourier_transform_2(CArray &x)
{
  // DFT
  unsigned int N = x.size(), k = N, n;
  double thetaT = 3.14159265358979323846264338328L / N;
  Complex phiT = Complex(cos(thetaT), -sin(thetaT)), T;
  TLOG() << "WHILE LOOP BEGINS" << std::endl;
  while (k > 1)
  {
    TLOG() << "k = " << k << std::endl;
    n = k;
    k >>= 1;
    phiT = phiT * phiT;
    T = 1.0L;
    for (unsigned int l = 0; l < k; l++)
    {
      for (unsigned int a = l; a < N; a += n)
      {
        unsigned int b = a + k;
        Complex t = x[a] - x[b];
        x[a] += x[b];
        x[b] = t * T;
      }
      T *= phiT;
    }
  }
  TLOG() << "WHILE LOOP ENDS, FOR LOOP BEGINS" << std::endl;
  // Decimate
  unsigned int m = (unsigned int)log2(N);
  for (unsigned int a = 0; a < N; a++)
  {
    TLOG() << "a = " << a << std::endl;
    unsigned int b = a;
    // Reverse bits
    b = (((b & 0xaaaaaaaa) >> 1) | ((b & 0x55555555) << 1));
    b = (((b & 0xcccccccc) >> 2) | ((b & 0x33333333) << 2));
    b = (((b & 0xf0f0f0f0) >> 4) | ((b & 0x0f0f0f0f) << 4));
    b = (((b & 0xff00ff00) >> 8) | ((b & 0x00ff00ff) << 8));
    b = ((b >> 16) | (b << 16)) >> (32 - m);
    if (b > a)
    {
      Complex t = x[a];
      x[a] = x[b];
      x[b] = t;
    }
  }
  TLOG() << "FOR LOOP ENDS" << std::endl;
}

void Fourier::compute_fourier(double rebin_factor)
{
  CArray input = fourier_prep(m_data);
  fast_fourier_transform_1(input);
  //fast_fourier_transform_2(input);
  int newsize = (int) input.size()/rebin_factor;
  CArray out_array (newsize);
  out_array = fourier_rebin(input, rebin_factor);
  m_rebin_factor = rebin_factor;
  m_fourier_transform.resize(newsize);
  for (size_t i = 0; i < out_array.size(); i++)
  {
    double val = (double) std::abs(out_array[i]);
    m_fourier_transform[i] = val;
  }
}
 
int Fourier::enter(double value, uint64_t time)
{
  if (m_start == -1) m_start = time;
  if ((m_end == -1) && (time - m_start >= m_duration)) 
  {
    m_end = time;
    return 1;
  }
  if (m_end != -1) return 1;

  uint64_t index64 = (time - m_start)/m_inc_size;
  if (index64 > m_npoints) 
  {
    std::cout << "-------OVERSPILL---------" << std::endl;
    std::cout << "index = ( time (" << time << ") - m_start (" << m_start << ") ) / m_inc_size (" << m_inc_size << ") = " << index64 << ", should be less than " << m_npoints << std::endl;
    std::cout << "Curr. time = " << time << ", should be less than end time = " << m_end << ". Difference = " << m_end - time << std::endl;
    std::cout << "m_inc_size = ( end (" << m_end << ") - start (" << m_start << ") ) / npoints (" << m_npoints << ")" << std::endl;
  }
  int index = (int) index64;
  if (index > m_npoints)
  {
    std::cerr << "Cannot enter a time that lies outside scope of series." << std::endl;
    return -1;
  }
  else m_data[index] = value;
  return 1;
}

double Fourier::find_value(uint64_t time)
{
  uint64_t index64 = (time - m_start)/m_inc_size;
  int index = (int) index64;
  if ((index > m_npoints) || (index < 0))
  {
    std::cerr << "Cannot find a time that lies outside scope of series." << std::endl;
    return -9999;
  }
  else return m_data[index];
}

void Fourier::save(const std::string &filename) const
{
  std::ofstream file;
  file.open(filename);
  file << m_start << " " << m_end << " " << m_inc_size << " " << m_npoints << " " << std::endl;
  for (auto x: m_data)
    file << x << " ";
  file << std::endl;
}

void Fourier::save(std::ofstream &filehandle) const
{
  filehandle << m_start << " " << m_end << " " << m_inc_size << " " << m_npoints << " " << std::endl;
  for (auto x: m_data)
    filehandle << x << " ";
  filehandle << std::endl;
}

void Fourier::save_fourier(const std::string &filename) const
{
  std::ofstream file;
  file.open(filename);
  file << m_start << " " << m_end << " " << m_inc_size << " " << m_npoints << " " << m_rebin_factor << std::endl;
  for (auto x: m_fourier_transform)
    file << x << " ";
  file << std::endl;
}

void Fourier::save_fourier(std::ofstream &filehandle) const
{
  filehandle << m_start << " " << m_end << " " << m_inc_size << " " << m_npoints << " " << m_rebin_factor << std::endl;
  for (auto x: m_fourier_transform)
    filehandle << x << " ";
  filehandle << std::endl;
}

//===================================================

class FourierLink : public AnalysisModule{
  std::string m_name;
  std::vector<Fourier> fouriervec;
  std::vector<std::string> chanvec;
  bool m_run_mark;

public:

  FourierLink(std::string name, int start, int end, int npoints);

  void run(dunedaq::dataformats::TriggerRecord &tr, std::string mode);
  void transmit(const std::string &topicname, int run_num, time_t timestamp, std::string chaninf) const;
  bool is_running();
};

FourierLink::FourierLink(std::string name, int start, int end, int npoints)
  : m_name(name), m_run_mark(false)
{
  for(int i=0; i<256; ++i){
    std::string chaninf = "NULL NULL NULL";
    //Fourier fourier(start, end, npoints);
    Fourier fourier(end - start, npoints);
    fouriervec.push_back(fourier);
    chanvec.push_back(chaninf);
  }
}

void FourierLink::transmit(const std::string &topicname, int run_num, time_t timestamp, std::string chaninf) const
{
  //TLOG() << "Beginning FFT transmission" << std::endl;
  std::stringstream csv_output;
  std::string datasource = "TESTSOURCE";
  std::string dataname   = this->m_name;
  std::string axislabel = "TESTLABEL";
  std::stringstream metadata;
  metadata << chaninf << " " << fouriervec[0].m_inc_size << " " << fouriervec[0].m_start << " " << fouriervec[0].m_end;

  //TLOG() << "Got strings" << std::endl; 

  int subrun = 0;
  int event = 0;
  
  //Construct CSV output
  csv_output << datasource << ";" << dataname << ";" << run_num << ";" << subrun << ";" << event << ";" << timestamp << ";" << metadata.str() << ";";
  csv_output << axislabel << "\n";
  for (int ich = 0; ich < 256; ++ich)
  {
    csv_output << chaninf << "\n";
    for (auto x: fouriervec[ich].m_data) csv_output << x << " ";
    csv_output << "\n";
  }
  //csv_output << "\n"; 
  
  //TLOG() << "Constructed output" << std::endl;
  
  //Transmit
  KafkaExport(csv_output.str(), topicname);

  //TLOG() << "Export complete" << std::endl;
}

void FourierLink::run(dunedaq::dataformats::TriggerRecord &tr, std::string mode){

  PdspChannelMapService channelMap("protoDUNETPCChannelMap_RCE_v4.txt", "protoDUNETPCChannelMap_FELIX_v4.txt");

  m_run_mark = true;

  //------------------------------------
  //MUCKING ABOUT
  
  std::vector<dataformats::WIBFrame*> wib_frames;
  std::vector<std::unique_ptr<dataformats::Fragment>>& fragments = tr.get_fragments_ref();
  for (auto &fragment:fragments)
  {
    int num_chunks = (fragment->get_size() - sizeof(dataformats::FragmentHeader)) / 464;

    for (int i = 0; i < num_chunks; i++)
    {
      dataformats::WIBFrame* frame = reinterpret_cast<dataformats::WIBFrame*> (static_cast<char*>(fragment->get_data()) + (i * 464));
      //TLOG() << "TIME FROM ALTERNATIVE: " << frame->get_wib_header()->get_timestamp() << std::endl;
      wib_frames.push_back(frame);
    } 
  }

  //------------------------------------
  dunedaq::dqm::Decoder dec;
  auto wibframes = dec.decode(tr);

  //int counter = 0;
  //uint64_t timeywimey = 0;
  //for(auto &fr:wibframes){
  for (auto &fr:wib_frames){
    //timeywimey = fr->get_wib_header()->get_timestamp();
    //if (counter == 0) TLOG() << "FIRST TIME = " << timeywimey << std::endl;
    //else TLOG() << "TIME = " << timeywimey << std::endl;
    //counter ++;
    for(int ich=0; ich<256; ++ich)
    {
      //Get channel info
      int crate = fr->get_wib_header()->crate_no;
      int slot = fr->get_wib_header()->slot_no;
      int fiber = fr->get_wib_header()->fiber_no;
      unsigned int fiberloc = FiberLoc(fiber);
      unsigned int crateloc = crate;
      unsigned int chloc = ich;
      if (chloc > 127)
      {
        chloc -= 128;
        fiberloc++;
      }
      //unsigned int offline = channelMap.GetOfflineNumberFromDetectorElements(crateloc, slot, fiberloc, chloc, PdspChannelMapService::kFELIX);
      unsigned int offline = ich;
      //unsigned int plane = channelMap.PlaneFromOfflineChannel(offline);
      //unsigned int apa = channelMap.APAFromOfflineChannel(offline);
      std::stringstream chan_info;
      //chan_info << apa << " " << plane << " " << offline;
      chan_info << offline;

      uint64_t timestamp = fr->get_wib_header()->get_timestamp();
      fouriervec[ich].enter(fr->get_channel(ich), timestamp);
      //fouriervec[ich].enter(fr.get_channel(ich), 0);
      chanvec[ich] = chan_info.str();
    }
  }
  //TLOG() << "Went through " << counter << "frames and last time was = " << timeywimey << std::endl;

  for (int ich=0; ich<256; ++ich)
  {
    fouriervec[ich].compute_fourier(1);
  }

  this->transmit("testdunedqm", tr.get_header_ref().get_run_number(), tr.get_header_ref().get_trigger_timestamp(), chanvec[0]);

  for(int ich=0; ich<256; ++ich)
    fouriervec[ich].save("Fourier/" + m_name + "-" + std::to_string(ich) + ".txt");
  m_run_mark = false;
}

bool FourierLink::is_running(){
  return m_run_mark;
}

} // namespace dunedaq::dqm
