import numpy as np
from scipy.fft import fft, fftfreq, rfft
from cmath import exp, pi

np.set_printoptions(linewidth=np.inf)

def fftnp(ary):
    N = len(ary)
    # nary = np.ones(N, dtype=np.cdouble)
    if N <= 1:
        return ary

    even = fftnp(ary[::2])
    odd = fftnp(ary[1::2])

    nary = np.zeros(N, dtype=np.cdouble)
    for k in range(N//2):
        p = even[k]
        q = np.exp(- 2 * np.pi * 1j * k / N) * (odd[k])
        nary[k] = p + q
        nary[k + N//2] = p - q
    return nary

def fftdef(ary):
    n = len(ary)
    nary = np.zeros(len(ary), dtype=np.cdouble)

    for k in range(len(ary)):
        nary[k] = np.sum(ary * np.exp(-2j * np.pi * k * np.arange(n)/n))
    return nary

def fftfreqdef(n, d):
    if n % 2 == 0:
        tmp = np.arange(n/2)
        tmp2 = np.arange(-n/2, 0)
        np.concatenate((tmp, tmp2))
    else:
        return 

def fftr(x):
    N = len(x)
    if N <= 1: return x
    even = fft(x[0::2])
    odd =  fft(x[1::2])
    T= [exp(-2j*pi*k/N)*odd[k] for k in range(N//2)]
    return [even[k] + T[k] for k in range(N//2)] + \
           [even[k] - T[k] for k in range(N//2)]

# First set of test cases
# N is the number of points
# T is the sample spacing

Nval = [200, 400, 600, 800, 1000]
Tval = [1/100, 1/500, 1/800, 1/800, 2]

for i, (N, T) in enumerate(zip(Nval, Tval), start=1):

    x = np.linspace(0.0, N*T, N, endpoint=False)
    y = np.sin(120.0 * 2.0*np.pi*x) + 0.5*np.sin(150.0 * 2.0*np.pi*x)

    xf = fftfreq(N, T)[:N//2]
    yf = fftdef(y)[0:N//2]

    # Uncomment for plotting
    # import matplotlib.pyplot as plt
    # plt.plot(xf, 2.0/N * np.abs(yf))
    # plt.show()

    f = open(f'Fourier-test-{i}-inp.txt', 'w')
    f.write(np.array_str(x)[1:-1] + '\n')
    f.write(np.array_str(y, precision=16)[1: -1])
    f.close()

    f = open(f'Fourier-test-{i}-out.txt', 'w')
    f.write(np.array_str(xf)[1:-1] + '\n')
    f.write(np.array_str(yf)[1:-1])
    f.close()
