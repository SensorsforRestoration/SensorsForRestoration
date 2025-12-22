#!/usr/bin/env python3
# ---- Simple PSH-based solar-battery simulator (hourly) ----
# Uses Peak Sun Hours (e.g., 4.9 h) instead of a 12 h daylight bell

import math
import numpy as np
import matplotlib.pyplot as plt

# ---------------------- PARAMETERS (EDIT) ----------------------
days_to_sim         = 30            # total days
dt_h                = 1.0           # 1 hour timestep

# Battery
battery_mAh         = 3000          # e.g., 6000 mAh
battery_V           = 3.7           # nominal pack voltage
usable_DoD          = 0.85          # use 85% of nameplate Wh
eta_chg             = 0.95          # charge efficiency
eta_dis             = 0.95          # discharge efficiency
soc_start           = 1.00          # start full

# Solar (PSH-style)
panel_Wp            = 4             # panel STC power (W)
psh_hours           = 4.9           # Peak Sun Hours per day (Egypt-like)
center_hour_local   = 12            # center the PSH block at solar noon
ambient_C           = 50.0          # hot worst-case
pv_temp_coeff       = -0.004        # -0.4%/°C above 25°C
bos_derate          = 0.90          # MPPT + wiring, etc.
extra_derate        = 0.85          # soiling/tilt/”partly cloudy” fudge

# Load (ESP baseline + hourly bursts)
load_base_W         = 0.20          # average baseline draw (W)
pump_W              = 10.0          # pump power (W)
pump_s_per_hour     = 10            # seconds ON each hour
lora_W              = 1.0           # LoRa TX power (W)
lora_s_per_hour     = 1             # seconds ON each hour
# --------------------------------------------------------------

# Derived
n_steps = int(days_to_sim * 24 / dt_h)
Wh_nom  = (battery_mAh / 1000.0) * battery_V
E_max   = Wh_nom * usable_DoD     # usable window (Wh)
E_min   = 0.0
E       = E_max * soc_start

# Temperature derate relative to 25°C
temp_der = max(0.0, 1.0 + pv_temp_coeff * (ambient_C - 25.0))
pv_derate_total = bos_derate * temp_der * extra_derate

# Hourly load energy (Wh)
def hourly_load_Wh():
    base = load_base_W * dt_h
    pump = pump_W * (pump_s_per_hour / 3600.0)
    lora = lora_W * (lora_s_per_hour / 3600.0)
    return base + pump + lora

E_load_per_h = hourly_load_Wh()     # Wh per hour (average)
avg_load_W   = E_load_per_h / dt_h

# PSH block evaluator: constant panel power during a psh_hours-wide window
def pv_power_W(hour_idx):
    h = (hour_idx % 24) + 0.5    # mid-hour
    half = psh_hours / 2.0
    start = center_hour_local - half
    end   = center_hour_local + half
    # wrap logic across midnight
    on = False
    if start >= 0 and end <= 24:
        on = (h >= start) and (h < end)
    else:
        # window crosses midnight
        on = (h >= (start % 24)) or (h < (end % 24))
    if not on:
        return 0.0
    # During PSH window, deliver (approximately) Wp * derates
    return panel_Wp * pv_derate_total

# Simulate
hours = np.arange(n_steps)
soc = []
brown = []
for t in hours:
    P_pv = pv_power_W(t)
    E_pv = max(0.0, P_pv * dt_h) * eta_chg
    E_ld = E_load_per_h / eta_dis

    E = min(E_max, max(E_min, E + E_pv - E_ld))
    soc.append(100.0 * (E / E_max if E_max > 0 else 0.0))
    brown.append(1 if (E <= E_min + 1e-9 and (E_pv - E_ld) < 0) else 0)

# Report
print("=== SIM SUMMARY ===")
print(f"Usable battery: {E_max:.2f} Wh  (from {battery_mAh} mAh @ {battery_V} V, DoD={usable_DoD:.2f})")
print(f"Panel: {panel_Wp} W  | PSH: {psh_hours} h/day  | Derate total: {pv_derate_total:.3f} (temp@{ambient_C}°C={temp_der:.3f})")
print(f"Avg load: {avg_load_W:.3f} W (incl. pump & LoRa bursts)")
print(f"Min SoC over {days_to_sim} d: {min(soc):.2f}%")
if any(brown):
    first = np.where(np.array(brown) == 1)[0][0]
    print(f"Brownout at hour ~{first}")
else:
    print("No brownouts.")

# Plot SoC
plt.figure(figsize=(10,5))
plt.plot(hours, soc)
plt.xlabel("Hours since start")
plt.ylabel("State of Charge (%)")
plt.title("Battery SoC vs Time — PSH Model (hourly)")
plt.grid(True)
plt.tight_layout()
plt.show()
