% To run in CLI, use matlab -batch "buck"
%% Define circuit parameters
V_in = 12;	% Volts
V_in_max = 12;	% Volts, max expected V_in, for clarity
V_in_min = 12;	% Volts, min expected V_in, for clarity
V_out = 5.3;	% Volts
I_out = 1.5;	% Amps
I_out_max = 2;	% Amps
C_derate = 0.7;	% 70% of capacitance at 3.7V DC

%% Datasheet constants
R_dc = 11 * 10^-3;		% Power inductor resistance
V_d = 0.7;                 	% Diode V_F B560C
V_out_sc = 0;               	% Output V during a short
R_ds_on = 92 * 10^-3;       	% High side FET R_ds when on
t_on = 135 * 10^-9;         	% Minimum on time for High side FET
f_div = 1;                  	% Divide cycle
I_cl = 5;                   	% Current limit
V_en = 1.2;			% Enable pin voltage reference
I_en = 1.2 * 10^-6;		% Enable pin pull-up current
V_ref = 0.8;			% Reference pin voltage

%% Switching frequency
f_sw_max = 1/t_on * (I_out * R_dc + V_out + V_d)/(V_in_max - I_out * R_ds_on + V_d);
f_sw_skip = f_div / t_on * (I_cl * R_dc + V_out_sc + V_d)/(V_in_max - I_cl * R_ds_on + V_d);
f_sw_Hz = 500 * 10^3;
f_sw_kHz = f_sw_Hz / 10^3;
% Max switching freq is ~ 3 MHz, IC max is 2.5 MHz
fprintf([
	'Higher switching frequency enables higher response time and lower component sizes, but more loss\n' ...
	'Lower switching frequency enables lower losses due to switching (heat) but requires larger components\n' ...
	'[Hard-coded] Middle ground: %d kHz\n' ...
	], f_sw_kHz)

%% Timing resistor
R_t_kOhms = 101756 / (f_sw_kHz^1.008);
R_t = R_t_kOhms * 1000;
fprintf("R_t is %d kOhms\n", R_t_kOhms)

%% Output inductor
% K_ind is the ratio between Inductor ripple current and max output current
% Inversely proportional to output capacitor ESR 
% We will likely use small, ceramics with low ESR, so we can use a val of 0.3.
K_ind = 0.3;
L_o_min = (V_in_max - V_out) / (I_out_max * K_ind) * (V_out)/(V_in_max * f_sw_Hz);
L_o = 10 * 10^-6; % Chosen based on closest to minimum, going lower
% Verify that's OK
I_ripple = (V_out * (V_in_max - V_out)) / (V_in_max * L_o * f_sw_Hz);
fprintf("[Hard-coded] Output inductor with %d of inductance produces %d current ripple, that is acceptable\n", L_o, I_ripple)
I_ind_rms = sqrt((I_out)^2 + 1/12 * ((V_out * (V_in_max - V_out))/(V_in_max * L_o * f_sw_Hz))^2);
I_ind_max = I_out + I_ripple/2;
fprintf("Output inductor has RMS current %d and max current draw %d\n", I_ind_rms, I_ind_max)

%% Output capacitor
V_i = V_out;									% Initial output voltage
I_O_H = I_out_max;								% Output current under heavy load, A, from power budget
I_O_L = 0; 									% Output current under light load, A
I_O_delta = I_O_H - I_O_L;							% Change in output current
I_ripple_max = (V_out * (V_in_max - V_out)) / (V_in_max * f_sw_Hz * L_o); 	% Max current ripple
V_f = V_out * 1.04;								% Max output voltage considering overshoot
V_out_delta = V_out * 0.04;							% Voltage margin
V_ripple_max = V_out * 0.005;							% Max voltage ripple

C_out_voltage_undershoot = (2 * I_O_delta) / (f_sw_Hz * V_out_delta);
C_out_voltage_overshoot = L_o * ((I_O_H^2 - I_O_L^2)/(V_f^2 - V_i^2));
C_out_voltage_ripple = 1/(8 * f_sw_Hz) * 1/(V_ripple_max/I_ripple_max);

fprintf([
	'Min output capacitance to minimize output voltage undershoot: %d\n' ...
	'Min output capacitance to minimize output voltage overshoot: %d\n' ...
	'Min output capacitance to minimize output voltage ripple: %d\n'
	], C_out_voltage_undershoot, C_out_voltage_overshoot, C_out_voltage_ripple)

fprintf("[Hard-coded] Output capacitance must be higher than above three. Let's choose: 3x 47 uF ceramics\n")
C_out_nominal = 3 * 47 * 10^-6;
C_out = C_out_nominal * C_derate;
C_esr_max = V_ripple_max / I_ripple_max;						% Max equivalent series resistance of output capacitors
I_c_rms_out = (V_out * (V_in_max - V_out)) / (sqrt(12) * V_in_max * f_sw_Hz * L_o);	% RMS AC current through output capacitors
fprintf("Max ESR for output caps: %d\n Max ripple current RMS: %d\n", C_esr_max, I_c_rms_out)

%% Catch Diode
fprintf("Diode must be rated for atleast: %d volts\n", V_in_max)
% Lower fwd voltage --> higher efficiency, schottkys are good
fprintf("The datasheet selects the B560C-13-F, we can use this as well.\n")
V_fwd = 0.535;			% This is at 1.5A
C_junction = 300 * 10^-12;	% Junction capacitance of a diode
P_diode = ((V_in_max - V_out) * I_out * V_fwd) / V_in_max + (C_junction * f_sw_Hz * (V_in + V_fwd)^2)/2;
fprintf("The diode will dissipate %.2f watts\n", P_diode)

%% Input Capacitance
fprintf("We will use 1 100uF electrolytic (far from supply - datasheet) and 2 2.2uF ceramic capacitors (decoupling)\n")
C_in_ceramic = 2.2*10^-6;
C_in_electrolytic = 100*10^-6;
C_in = C_in_electrolytic + C_in_ceramic * 2;
C_in_effective = C_in_ceramic * 2 * C_derate;
R_esr = 1.25 * 10^-3 / 3;								% 1.25 mOhms, 3 in parallel
D = V_out / V_in_min;
I_c_rms_in = I_out_max * sqrt(D * (1-D));						% RMS AC current through input capacitors
V_in_delta = (I_out * 0.25) / (C_in_effective * f_sw_Hz);
fprintf("AC current through input caps: %d\n Input voltage ripple: %d\n", I_c_rms_in, V_in_delta)
%% Bootstrap Capacitor
fprintf("Connect a 0.1uF ceramic between BOOT and SW\n")
%% UVLO (Under Voltage Lockout) Setpoint
V_start = 6.5;				% Upon startup, don't start switching until we reach V_start
V_stop = 5;				% When shutting down, keep switching until we reach V_stop
I_hysteresis = 3.4 * 10^-6;		% Goes through R_UVLO_1 to keep EN high and continue switching

R_UVLO_1 = (V_start - V_stop) / I_hysteresis;
R_UVLO_2 = V_en / ((V_start - V_en) / R_UVLO_1 + I_en);

fprintf([
	'UVLO Resistor 1 (Between VIN and EN): %d\n.' ...
	'UVLO Resistor 2 (Between EN and GND): %d\n.' ...
	], R_UVLO_1, R_UVLO_2)

%% Output Voltage & Feedback Resistors
R_low_side = 10.2 * 10^3;
R_high_side = R_low_side * (V_out - 0.8) / 0.8;

fprintf("R5 (Btwn V_out and FB): %d\n, R6 (Btwn FB and GND): %d\n", R_high_side, R_low_side)

%% Minimum VIN
% Going to skip this since the PDB will give 12V whether we like it or not.

%% Compensation
f_modulator_pole = I_out_max / (2 * pi * V_out * C_out);
f_modulator_zero = 1 / (2 * pi * R_esr * C_out);
f_co_1 = sqrt(f_modulator_pole * f_modulator_zero);
f_co_2 = sqrt(f_modulator_pole * f_sw_Hz/2);
fprintf([
	'Given Compensation frequencies 1 and 2: %.2f, %.2f\n' ...
	'We will choose f_co=13kHz' ...
	], f_co_1, f_co_2)
f_comp_Hz = 13 * 10^3;
gmps = 17;			% Power stage transconductance, A/V
gmea = 350 * 10^-6;		% Error amp transconductance, uA/V
% Transconductance amp is an amp that takes in a voltage and outputs a proportional current
% For power stage, the buck will read V_comp and keep the high-side FET on until the inductor
% reaches appropriate current. For the error amp, it will compare V_FB to V_REF and output
% a proportional current to the COMP pin.
R_comp = (2 * pi * f_comp_Hz * C_out) / gmps * V_out / (V_ref * gmea);
C_comp = 1 / (2 * pi * R_comp * f_modulator_pole);
C_comp_2 = max((C_out * R_esr) / R_comp, 1/(R_comp * f_sw_Hz * pi));
fprintf("Compensation resistor: %.2f\n Compensation capacitor: %.9f\n", R_comp, C_comp)
fprintf("Optional additional compensation capacitor: %.15f\n", C_comp_2)

%% Power Dissipation
t_rise = (V_in * 0.16 + 3) * 10^-9;				% Rise time of internal FET
Q_g = 3 * 10^-9;						% Gate charge of internal FET
I_q = 175 * 10^-6;						% Nonswitching supply current
P_cond = (I_out_max)^2 * R_ds_on * (V_out / V_in);		% Power dissipated from conduction
P_sw = V_in * f_sw_Hz * I_out_max * t_rise;			% Power dissipated from switching
P_gd = V_in * Q_g * f_sw_Hz;					% Power dissipated from gate drive
P_q = V_in * I_q;						% Power dissipated from supply current
P_total_ic = P_cond + P_sw + P_gd + P_q;			% IC power dissipation
P_total = P_total_ic + P_diode;					% Total power dissipation
fprintf("Total power lost in the IC is: %.2f watts\n", P_total_ic)
fprintf("Total power lost in the systme is: %.2f watts\n", P_total)

fprintf([
	'===== SUMMARY =====\n' 			...
	'For Input V: %.2f\t and Output V: %.2f\n'	...
	'Caps ceramic unless otherwise indicated\n' 	...
	'C1: 2.2uF\n' 					...
	'C2: 2.2uF\n' 					...
	'C3: 2.2uF\n' 					...
	'C4: 0.1uF\n' 					...
	'C5: 29nF\n'					...
	'C6: 47uF\n'					...
	'C7: 47uF\n'					...
	'C8: %.15f (Do not populate) \n'		...
	'C9: 47uF\n'					...
	'C10: 100uF (Electrolytic)\n' 			...
	'R1: %.2f -> 442kOhm\n'				...
	'R2: %.2f -> 90.9kOhm\n' 			...
	'R3: %.2f -> 193kOhm\n'				...
	'R4: %.2f -> 6.34kOhm\n'			...
	'R5: %.2f -> 37kOhm\n'				...
	'R6: %.2f -> 10.2kOhm\n'			...
	'L1: %.9f\n' 					...
	'D1: B560C\n' 					...
	], V_in, V_out, C_comp_2, R_UVLO_1, R_UVLO_2, R_t, R_comp, R_high_side, R_low_side, L_o)

