%% Define circuit parameters
V_in = 12;	% Volts
V_in_max = 12;	% Volts, max expected V_in, for clarity
V_out = 3.7;	% Volts
I_out = 5;	% Amps 

%% Datasheet constants
R_dc = 11 * 10^-3;		% Power inductor resistance
V_d = 0.7;                 	% Diode V_F B560C
V_out_sc = 0;               	% Output V during a short
R_ds_on = 92 * 10^-3;       	% High side FET R_ds when on
t_on = 135 * 10^-9;         	% Minimum on time for High side FET
f_div = 1;                  	% Divide cycle
I_cl = 5;                   	% Current limit

%% Switching frequency
f_sw_max = 1/t_on * (I_out * R_dc + V_out + V_d)/(V_in_max - I_out * R_ds_on + V_d);
f_sw_skip = f_div / t_on * (I_cl * R_dc + V_out_sc + V_d)/(V_in_max - I_cl + R_ds_on + V_d);
f_sw_Hz = 500 * 10^3;
f_sw_kHz = f_sw_Hz / 10^3;
% Max switching freq is ~ 3 MHz, IC max is 2.5 MHz
fprintf([
	'Higher switching frequency enables higher response time and lower component sizes, but more loss\n' ...
	'Lower switching frequency enables lower losses due to switching (heat) but requires larger components\n' ...
	'Middle ground: %d kHz\n' ...
	], f_sw_kHz)

%% Timing resistor
R_t_kOhms = 101756 / (f_sw_kHz^1.008);
fprintf("R_t is %d kOhms\n", R_t_kOhms)

%%Output inductor
% K_ind is the ratio between Inductor ripple current and max output current
% Inversely proportional to output capacitor ESR 
% We will likely use small, ceramics with low ESR, so we can use a val of 0.3.
K_ind = 0.3;
L_o_min = (V_in_max - V_out) / (I_out * K_ind) * (V_out)/(V_in_max * f_sw_Hz);
L_o = 3.3 * 10^-6; % Chosen based on closest to minimum, going lower
% Verify that's OK
I_ripple = (V_out * (V_in_max - V_out)) / (V_in_max * L_o * f_sw_Hz);
fprintf("Output inductor with %d of inductance produces %d current ripple, that is acceptable\n", L_o, I_ripple)
I_ind_rms = sqrt((I_out)^2 + 1/12 * ((V_out * (V_in_max - V_out))/(V_in_max * L_o * f_sw_Hz))^2);
I_ind_max = I_out + I_ripple/2;
fprintf("Output inductor has RMS current %d and max current draw %d\n", I_ind_rms, I_ind_max)




