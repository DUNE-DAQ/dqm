import numpy as np
from scipy.fft import fft, rfftfreq, rfft
from cmath import exp, pi

np.set_printoptions(linewidth=np.inf)

# First set of test cases
# N is the number of points
# T is the sample spacing

Nval = [200,   400,   600,   800,   1000, 355]
Tval = [1/100, 1/500, 1/800, 1/800, 2,    1.3]

for i, (N, T) in enumerate(zip(Nval, Tval), start=1):

    x = np.linspace(0.0, N*T, N, endpoint=False)
    y = np.sin(120.0 * 2.0*np.pi*x) + 0.5*np.sin(150.0 * 2.0*np.pi*x)

    xf = rfftfreq(N, T)
    yf = np.abs(rfft(y))

    print(xf.shape, yf.shape)
    print(xf)

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
