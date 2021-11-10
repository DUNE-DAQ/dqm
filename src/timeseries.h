///////////////////////////////////////////
//
// Custom time series class
//
///////////////////////////////////////////

#include <string>
#include <vector>

#include <complex>
#include <iostream>
#include <valarray>

typedef std::complex<double> Complex;
typedef std::valarray<Complex> CArray;

class TimeSeries
{

  double FindValue(uint64_t time);
  CArray FourierPrep(std::vector<double> input);
  CArray FourierRebin(CArray input, double factor);
  void FastFourierTransform(CArray& x);

public:
  uint64_t fStart, fEnd, fIncSize;
  int fNPoints;
  std::vector<double> fData;
  double fRebinFactor;
  // CArray fFourierTransform (10000);
  std::vector<double> fFourierTransform;

  TimeSeries(uint64_t start, uint64_t end, int npoints);
  int Enter(double value, uint64_t time);
  void ComputeFourier(double rebin_factor);

  void Save(std::string filename);
  void Save(std::ofstream& filehandle);

  void SaveFourier(std::string filename);
  void SaveFourier(std::ofstream& filehandle);
};
