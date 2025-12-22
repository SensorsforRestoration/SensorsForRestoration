# Battery calculator — explanation

This document explains `build/tools/battery_calculator.py`: a small hourly solar+battery simulator that uses a Peak Sun Hours (PSH) block model to estimate battery State‑of‑Charge (SoC) over multiple days.

What it does

- Simulates PV energy (a flat power block for the daily PSH window), charges the battery (with charge efficiency), subtracts hourly loads (with discharge efficiency), clamps usable energy to a DoD window, and records SoC and simple "brownout" events.
- Prints a short summary (usable Wh, panel/PSH, avg load, min SoC, brownout hour if any) and plots SoC vs time.

Key parameters (edit at top of the script)

- Simulation: `days_to_sim`, `dt_h` (hours per step).
- Battery: `battery_mAh`, `battery_V`, `usable_DoD`, `eta_chg`, `eta_dis`, `soc_start`.
- Solar: `panel_Wp`, `psh_hours`, `center_hour_local`, `ambient_C`, `pv_temp_coeff`, `bos_derate`, `extra_derate`.
- Loads: `load_base_W`, `pump_W`, `pump_s_per_hour`, `lora_W`, `lora_s_per_hour`.

Important derived quantities

- `Wh_nom = (battery_mAh / 1000) * battery_V` — battery nominal energy (Wh).
- `E_max = Wh_nom * usable_DoD` — usable Wh window for the simulation.
- `pv_derate_total = bos_derate * temp_der * extra_derate` — total PV derate factor including temperature.
- `E_load_per_h` — combined hourly energy of base, pump and LoRa bursts.

Model details

- PV: A contiguous `psh_hours` block centered at `center_hour_local` delivers `panel_Wp * pv_derate_total` W during each hour of the block; outside the block PV=0. Charging is reduced by `eta_chg`.
- Loads: Hourly energy is computed from baseline Watts and burst durations (pump and LoRa) and adjusted for `eta_dis` when drawing from the battery.
- Battery: Tracked as a single energy bucket `E` clipped to [0, E_max]. SoC is `100 * E / E_max`.
- Brownout: flagged when `E` reaches 0 and net for the hour is negative (battery depleted while load exceeds PV).

How to run

- Requires Python packages: `numpy` and `matplotlib`.
- From repo root:

```bash
python build/tools/battery_calculator.py
```

Output

- Console summary with usable Wh, panel/PSH, avg load, min SoC and brownout hour (if any).
- A plot window showing SoC (%) vs hours since start.

Limitations

- PSH block is a coarse model (flat output during the block) — it does not model sunrise/sunset curves or intra‑hour cloud variability.
- Battery is an energy bucket (no voltage curve, internal resistance, or rate‑dependent losses).
- Loads are converted to hourly energy; instantaneous current spikes are not modeled.

Suggestions for extension

- Replace PSH block with a diurnal curve (cosine/Gaussian) that integrates to PSH hours.
- Add CSV output and a command‑line interface for batch runs.
- Model battery voltage/IR or use a lookup table for DoD vs available energy vs current.
