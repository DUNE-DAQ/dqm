/**
 * @file Fourier.hpp Fast fourier transform using the Cooley-Tukey algorithm
 *
 * This is part of the DUNE DAQ , copyright 2020.
 * Licensing/copyright details are in the COPYING file that you should have
 * received with this code.
 */
#ifndef DQM_SRC_FOURIER_HPP_
#define DQM_SRC_FOURIER_HPP_

// dqm
#include "AnalysisModule.hpp"
#include "Decoder.hpp"
#include "Exporter.hpp"
#include "dqm/Types.hpp"

#include "dataformats/TriggerRecord.hpp"
#include "logging/Logging.hpp"

#include <complex>
#include <fstream>
#include <iostream>
#include <ostream>
#include <string>
#include <valarray>
#include <vector>

namespace dunedaq::dqm {

typedef std::complex<double> Complex;
typedef std::valarray<Complex> CArray;

class Fourier
{

  double find_value(uint64_t time); // NOLINT(build/unsigned)
  CArray fourier_prep(const std::vector<double>& input) const;
  CArray fourier_rebin(CArray input, double factor);
  void fast_fourier_transform_1(CArray& x);
  void fast_fourier_transform_2(CArray& x);


public:
  uint64_t m_start;    // NOLINT(build/unsigned)
  uint64_t m_end;      // NOLINT(build/unsigned)
  uint64_t m_inc_size; // NOLINT(build/unsigned)
  double m_freq_max;
  int m_npoints;
  std::vector<double> m_data;
  double m_rebin_factor;
  // CArray m_fourier_transform (10000);
  std::vector<double> m_fourier_transform;

  Fourier(uint64_t start, uint64_t end, int npoints); // NOLINT(build/unsigned)
  int enter(double value, uint64_t time);             // NOLINT(build/unsigned)
  void compute_fourier(double rebin_factor);

  void save(const std::string& filename) const;
  void save(std::ofstream& filehandle) const;

  void save_fourier(const std::string& filename) const;
  void save_fourier(std::ofstream& filehandle) const;
};

Fourier::Fourier(uint64_t start, uint64_t end, int npoints) // NOLINT(build/unsigned)
//  : m_start(start), m_end(end), m_npoints(npoints)
{
  m_start = start;
  m_end = end;
  m_npoints = npoints;

  // Unused
  // uint64_t npoints64 = (uint64_t) npoints; // NOLINT(build/unsigned)
  // m_inc_size = (end - start)/npoints64;
  m_inc_size = 25;
  m_data = std::vector<double>(npoints, 0);
}

CArray
Fourier::fourier_prep(const std::vector<double>& input) const
{
  std::valarray<Complex> output(input.size());
  Complex val;
  for (size_t i = 0; i < input.size(); i++) {
    val = input[i];
    output[i] = val;
  }
  return output;
}

CArray
Fourier::fourier_rebin(CArray input, double factor)
{
  // Unused
  int newsize = static_cast<int>(input.size() / factor);
  std::valarray<Complex> output(newsize);
  for (size_t i = 0; i < input.size(); i++) {
    int k = static_cast<int>(i) / factor;
    // std::cout << "i = " << i << ", k = " << k << std::endl;
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
void
Fourier::fast_fourier_transform_2(CArray& x)
{
  // DFT
  unsigned int N = x.size(), k = N, n;
  double thetaT = 3.14159265358979323846264338328L / N;
  Complex phiT = Complex(cos(thetaT), -sin(thetaT)), T;
  while (k > 1) {
    n = k;
    k >>= 1;
    phiT = phiT * phiT;
    T = 1.0L;
    for (unsigned int l = 0; l < k; l++) {
      for (unsigned int a = l; a < N; a += n) {
        unsigned int b = a + k;
        Complex t = x[a] - x[b];
        x[a] += x[b];
        x[b] = t * T;
      }
      T *= phiT;
    }
  }
  // Decimate
  unsigned int m = (unsigned int)log2(N);
  for (unsigned int a = 0; a < N; a++) {
    unsigned int b = a;
    // Reverse bits
    b = (((b & 0xaaaaaaaa) >> 1) | ((b & 0x55555555) << 1));
    b = (((b & 0xcccccccc) >> 2) | ((b & 0x33333333) << 2));
    b = (((b & 0xf0f0f0f0) >> 4) | ((b & 0x0f0f0f0f) << 4));
    b = (((b & 0xff00ff00) >> 8) | ((b & 0x00ff00ff) << 8));
    b = ((b >> 16) | (b << 16)) >> (32 - m);
    if (b > a) {
      Complex t = x[a];
      x[a] = x[b];
      x[b] = t;
    }
  }
}

void
Fourier::compute_fourier(double rebin_factor)
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

  double f_clock = 50E6;
  double f_sampling = f_clock/(int)m_inc_size;
  m_freq_max = f_sampling/2.0;
}

int
Fourier::enter(double value, uint64_t time) // NOLINT(build/unsigned)
{
  uint64_t index64 = (time - m_start) / m_inc_size; // NOLINT(build/unsigned)
  if (index64 > static_cast<uint64_t>(m_npoints))   // NOLINT(build/unsigned)
  {
    TLOG() << "-------OVERSPILL---------";
    TLOG() << "index = ( time (" << time << ") - m_start (" << m_start << ") ) / m_inc_size (" << m_inc_size
           << ") = " << index64 << ", should be less than " << m_npoints;
    TLOG() << "Curr. time = " << time << ", should be less than end time = " << m_end
           << ". Difference = " << m_end - time;
    TLOG() << "m_inc_size = ( end (" << m_end << ") - start (" << m_start << ") ) / npoints (" << m_npoints << ")";
  }

  int index = static_cast<int>(index64);
  if (index > m_npoints) {
    TLOG() << "Cannot enter a time that lies outside scope of series.";
    return -1;
  } else {
    m_data[index] = value;
  }

  return 1;
}

double
Fourier::find_value(uint64_t time) // NOLINT(build/unsigned)
{
  uint64_t index64 = (time - m_start) / m_inc_size; // NOLINT(build/unsigned)
  int index = static_cast<int>(index64);
  if ((index > m_npoints) || (index < 0)) {
    TLOG() << "Cannot find a time that lies outside scope of series.";
    return -9999;
  } else {
    return m_data[index];
  }
}

void
Fourier::save(const std::string& filename) const
{
  std::ofstream file;
  file.open(filename);
  file << m_start << " " << m_end << " " << m_inc_size << " " << m_npoints << " " << std::endl;
  for (auto x : m_data) {
    file << x << " ";
  }
  file << std::endl;
}

void
Fourier::save(std::ofstream& filehandle) const
{
  filehandle << m_start << " " << m_end << " " << m_inc_size << " " << m_npoints << " " << std::endl;
  for (auto x : m_data) {
    filehandle << x << " ";
  }
  filehandle << std::endl;
}

void
Fourier::save_fourier(const std::string& filename) const
{
  std::ofstream file;
  file.open(filename);
  file << m_start << " " << m_end << " " << m_inc_size << " " << m_npoints << " " << m_rebin_factor << std::endl;
  for (auto x : m_fourier_transform) {
    file << x << " ";
  }
  file << std::endl;
}

void
Fourier::save_fourier(std::ofstream& filehandle) const
{
  filehandle << m_start << " " << m_end << " " << m_inc_size << " " << m_npoints << " " << m_rebin_factor << std::endl;
  for (auto x : m_fourier_transform) {
    filehandle << x << " ";
  }
  filehandle << std::endl;
}

class FourierLink : public AnalysisModule
{
  std::string m_name;
  std::vector<Fourier> fouriervec;
  bool m_run_mark;

public:
  FourierLink(std::string name, int start, int end, int npoints);

  void run(dunedaq::dataformats::TriggerRecord& tr, RunningMode mode, std::string kafka_address);
  void transmit(std::string &kafka_address, const std::string &topic_name, int run_num, time_t timestamp);
  bool is_running();
};

FourierLink::FourierLink(std::string name, int start, int end, int npoints)
  : m_name(name)
  , m_run_mark(false)
{
  for (int i = 0; i < 256; ++i) {
    Fourier fourier(start, end, npoints);
    fouriervec.push_back(fourier);
  }
}

void
FourierLink::run(dunedaq::dataformats::TriggerRecord& tr, RunningMode, std::string kafka_address)
{
  m_run_mark = true;
  dunedaq::dqm::Decoder dec;
  auto wibframes = dec.decode(tr);

  for (auto fr : wibframes) {
    uint64_t timestamp = fr->get_wib_header()->get_timestamp();
    for (int ich = 0; ich < 256; ++ich) {
      //Fill time series
      fouriervec[ich].enter(fr->get_channel(ich), timestamp);
    }
  }

  for (int ich = 0; ich < 256; ++ich) {
    //Perform Fourier transform
    fouriervec[ich].compute_fourier(1);
    //Save text output
    fouriervec[ich].save("Fourier/" + m_name + "-" + std::to_string(ich) + ".txt");
  }

  //Transmit via kafka
  transmit(kafka_address, "testdunedqm", tr.get_header_ref().get_run_number(), tr.get_header_ref().get_trigger_timestamp());
  
  m_run_mark = false;
}

bool
FourierLink::is_running()
{
  return m_run_mark;
}

void
FourierLink::transmit(std::string &kafka_address, const std::string &topic_name, int run_num, time_t timestamp)
{
  std::stringstream csv_output;
  std::string datasource = "TESTSOURCE";
  std::string dataname   = this->m_name;
  std::string axislabel = "TESTLABEL";
  std::stringstream metadata;
  //metadata << chaninf << " " << fouriervec[0].m_inc_size << " " << fouriervec[0].m_start << " " << fouriervec[0].m_end << " " << fouriervec[0].m_freq_max;
  metadata << fouriervec[0].m_inc_size << " " << fouriervec[0].m_start << " " << fouriervec[0].m_end << " " << fouriervec[0].m_freq_max;

  int subrun = 0;
  int event = 0;

  int fft_size = fouriervec[0].m_fourier_transform.size();
  
  //Construct CSV output
  csv_output << datasource << ";" << dataname << ";" << run_num << ";" << subrun << ";" << event << ";" << timestamp << ";" << metadata.str() << ";";
  csv_output << axislabel << "\n";
  for (int ich = 0; ich < 256; ++ich)
  {
    csv_output << "Fourier_" << ich+1 << "\n";
    for (int i = 0; i < fft_size/2; i++) csv_output << fouriervec[ich].m_fourier_transform[i] << " ";
    csv_output << "\n";
  }

  KafkaExport(kafka_address, csv_output.str(), topic_name);
}


} // namespace dunedaq::dqm

#endif // DQM_SRC_FOURIER_HPP_
