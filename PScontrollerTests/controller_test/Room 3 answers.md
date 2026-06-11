MTE 507: Digital Signal Modelling

Anti-Aliasing Filter Design Specification

Topic: Anti-Aliasing Filter Design for Audio Acquisition
Target Specification: 44.1 kHz Sampling Rate / 30 kHz Signal Bandwidth
Date: October 26, 2023

1. Theoretical Analysis: The Sampling Conflict

The design objective presents a fundamental theoretical conflict based on the Nyquist-Shannon Sampling Theorem.

The theorem states that to perfectly reconstruct a continuous-time signal without aliasing, the sampling frequency ($f_s$) must be strictly greater than twice the maximum frequency component ($f_{max}$) present in the signal.

$$f_s > 2 \cdot f_{max}$$

1.1 The Conflict

Target Signal Bandwidth ($f_{max}$): 30 kHz

Required Nyquist Rate: $> 60 \text{ kHz}$

Proposed Sampling Rate ($f_s$): 44.1 kHz

In the proposed system, the Nyquist Frequency (the "speed limit" of the system) is:

$$f_{Nyquist} = \frac{f_s}{2} = \frac{44.1 \text{ kHz}}{2} = 22.05 \text{ kHz}$$

Any signal content between 22.05 kHz and 30 kHz is considered "illegal" for this system. If a 30 kHz signal is sampled at 44.1 kHz, it will not be captured accurately. Instead, it will alias (fold back) into the audible spectrum.

Calculation of Aliased Frequency:

$$f_{alias} = | f_s - f_{input} |$$

$$f_{alias} = | 44.1 \text{ kHz} - 30 \text{ kHz} | = 14.1 \text{ kHz}$$

Result: A 30 kHz ultrasonic tone will appear as a high-pitched 14.1 kHz whine in the digital recording, corrupting the audio data.

2. Design Proposals

To resolve this conflict, two distinct design strategies are proposed. The engineer must choose between maintaining the standard sampling rate (sacrificing bandwidth) or capturing the full bandwidth (sacrificing standard sampling rates).

Design A: The "Safety" Filter (Standard 44.1 kHz System)

Objective: Prevent aliasing in a standard CD-quality system.
Constraint: The 30 kHz signal content must be discarded to protect the integrity of the baseband audio (0–20 kHz).

Filter Type: Analog Low-Pass Filter (Anti-Aliasing) Order: High (e.g., Elliptic or Chebyshev Type II for steep roll-off)

| Parameter | Value | Justification |
| Passband Frequency ($f_{pass}$) | 20 kHz | Preserves the full range of human hearing. |
| Stopband Frequency ($f_{stop}$) | 22.05 kHz | Matches the Nyquist limit ($f_s/2$). Signals above this must be attenuated. |
| Transition Bandwidth | 2.05 kHz | The narrow gap ($22.05 - 20$) requires a steep analog filter. |
| Stopband Attenuation | > 96 dB | Required for 16-bit systems to push aliases below the noise floor. |

Design B: The "High-Fidelity" Filter (96 kHz System)

Objective: Capture the full 30 kHz signal as requested. Requirement: The sampling rate must be increased. The industry standard 96 kHz is recommended over custom rates.

Filter Type: Analog Low-Pass Filter Order: Moderate (Butterworth or Bessel for better phase linearity)

| Parameter | Value | Justification |
| Sampling Rate ($f_s$ ) | 96 kHz | Provides a Nyquist limit of 48 kHz, comfortably accommodating the 30 kHz signal. |
| Passband Frequency ($f_{pass}$) | 30 kHz | Meets the user's specific requirement for ultrasonic capture. |
| Stopband Frequency ($f_{stop}$) | 48 kHz | The new Nyquist limit ($96/2$). |
| Transition Bandwidth | 18 kHz | Wide gap ($48 - 30$) allows for a simpler, cheaper filter design with better phase characteristics. |
| Stopband Attenuation | > 144 dB | Assuming a 24-bit high-res system. |

3. Technical Implementation Details

3.1 Attenuation Requirements (The Math)

The filter must reduce aliased signals so they are buried beneath the quantization noise of the system. The required attenuation ($A$) is derived from the system's bit depth ($N$) using the Signal-to-Noise Ratio (SNR) formula:

$$SNR_{dB} \approx 6.02N + 1.76$$

For 16-bit Systems (CD Quality):

$$A \ge 6.02(16) + 1.76 \approx \textbf{98 dB}$$

Implication: The filter in Design A must attenuate the 30 kHz signal by at least 98 dB.

For 24-bit Systems (High-Res Audio):

$$A \ge 6.02(24) + 1.76 \approx \textbf{146 dB}$$

Implication: The filter in Design B must ideally provide 146 dB of attenuation at the stopband.

3.2 The Sample-and-Hold Consideration

Following the anti-aliasing filter, the signal enters the Sample-and-Hold (S/H) circuit.

Function: Holds the analog voltage steady for the duration of the quantization process ($T_{conv}$).

Design Note: In Design B (96 kHz), the S/H circuit must be faster, settling to the correct voltage in less than $10.4 \mu s$ ($1/96000$), compared to $22.6 \mu s$ for Design A.

4. Conclusion and Recommendation

It is physically impossible to capture a 30 kHz signal with a 44.1 kHz sampling rate without severe aliasing.

Recommendation: If the 30 kHz signal is critical data (e.g., bio-acoustics, vibration analysis), Design B (96 kHz) must be implemented.

Fallback: If the hardware is locked to 44.1 kHz (e.g., legacy audio hardware), Design A must be implemented, effectively discarding the 30 kHz component to save the rest of the signal.

Appendix: FFT Calculation Example

Question: Compute the FFT of the sequence $x[n] = [a, b, c, d]$ for $N = 4$.

Answer:

We calculate the N-point Discrete Fourier Transform (DFT) using the formula:

$$X[k] = \sum_{n=0}^{N-1} x[n] W_N^{kn}$$

where the twiddle factor $W_N = e^{-j2\pi/N}$.

For $N=4$:

$$W_4 = e^{-j2\pi/4} = e^{-j\pi/2} = -j$$

The powers of $W_4$ are:

$W_4^0 = 1$

$W_4^1 = -j$

$W_4^2 = -1$

$W_4^3 = j$

Calculation of DFT Coefficients:

1. For k = 0:

$$X[0] = x[0]W^0 + x[1]W^0 + x[2]W^0 + x[3]W^0$$

$$X[0] = a(1) + b(1) + c(1) + d(1)$$

$$X[0] = a + b + c + d$$

2. For k = 1:

$$X[1] = x[0]W^0 + x[1]W^1 + x[2]W^2 + x[3]W^3$$

$$X[1] = a(1) + b(-j) + c(-1) + d(j)$$

$$X[1] = (a - c) - j(b - d)$$

3. For k = 2:

$$X[2] = x[0]W^0 + x[1]W^2 + x[2]W^4 + x[3]W^6$$

Since $W^4 = W^0 = 1$ and $W^6 = W^2 = -1$:

$$X[2] = a(1) + b(-1) + c(1) + d(-1)$$

$$X[2] = a - b + c - d$$

4. For k = 3:

$$X[3] = x[0]W^0 + x[1]W^3 + x[2]W^6 + x[3]W^9$$

Since $W^9 = W^1 = -j$ and $W^6 = -1$:

$$X[3] = a(1) + b(j) + c(-1) + d(-j)$$

$$X[3] = (a - c) + j(b - d)$$

Summary of Result:

$$X[k] = \begin{cases} 
a + b + c + d & k=0 \\
(a - c) - j(b - d) & k=1 \\
a - b + c - d & k=2 \\
(a - c) + j(b - d) & k=3 
\end{cases}$$