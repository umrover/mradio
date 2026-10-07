%% Circuit Parameters
V_in = 3.7;
V_out = 3.3;
% The below should all be populated by the end of the script
C_bias = 0;		% Bias pin not used (not doing LILO)
% Don't need feedback resistors for ANY-OUT
R_1 = 11.8 * 10^3;	% (FB) ~11.8kOhm per datasheet
R_2 = 3.74 * 10^3;	% (FB) ~3.74kOhm per datasheet

% TODO: Look any-out mode vs regular (both offer 3v3 it seems)

%% Input Capacitance
fprintf("1x47uF and 2x10uF at the input\n")
C_in = 48 * 10^-6 + 2 * 10 * 10^-6;

%% Output Capacitance
fprintf("1x47uF, 1x10uF, 1x1uF, 1x0.1uF at the output\n")
C_out = 47 * 10^-6 + 10 * 10^-6 + 1 + 10^-6 + + 0.1 * 10^-6;

%% Feedforward Capacitance
fprintf("10nF feedforward -- don't need for ANY0OUT\n")
C_ff = 10 * 10^-9;

%% Noise Reduction/Soft-Start Capacitance
I_nr_ss = 6.2;
V_nr_ss = 0.8;
C_nr_ss = 100 * 10^-9;
t_ss = (V_nr_ss * C_nr_ss) / I_nr_ss;
fprintf("100nF NR/SS capacitance, can tune this\n")

%% Power Good
fprintf("2.2kOhm to drive LED circuit (regular PGood circuit") 
