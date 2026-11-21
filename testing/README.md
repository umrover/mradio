# Link test harness

`harness.py` flashes a board with the HaLow link test, logs every plugged-in ST-LINK console with a host timestamp, and writes a report that updates while the run goes.

- The firmware side is `CM7/Core/Src/link_test.cpp`, built in with `-DLINK_TEST=ON`.
  - Every 5 s each board prints one `link test ap:` or `link test sta:` line.
  - The line has tx and rx throughput, flood packets and loss, ping replies and RTT, and on the STA its RSSI and busiest MCS.
- One board floods UDP at the other on port 5001. By default the STA floods and the AP receives, since video flows rover to base.

## Commands

| Command | Action |
| :-- | :-- |
| `./flash_ap.sh` | Build the AP link test and flash it to the first ST-LINK |
| `./flash_sta.sh` | Build the STA link test and flash it to the first ST-LINK |
| `./harness.py` | Ask for a test name and a length, open the live report, then capture |
| `./harness.py --name "site 1" --minutes 30` | Capture without the questions |
| `./harness.py report results/<run>` | Rebuild the report of a past run |
| `python3 test_harness.py` | Check the parser, the stop table and the report |
| `./clear_results.sh` | List every run in `results/` and delete them after a yes, `-y` skips the question |

- The capture length defaults to 1 minute, and Ctrl-C ends it early. `--reset` resets the local boards first, and `--no-open` only prints the report link.
- The report opens in the default browser and reloads every 5 s until the capture ends. The `file://` link is printed too.

`flash_ap.sh` and `flash_sta.sh` pass any options on to `harness.py flash`.

- `--index 1` picks the second ST-LINK when both boards are plugged in.
- `--sub-band low|mid|high` and `--bw 1|2|4|8`, default mid at 8 MHz. Both boards need the same values.
- `--tx-power` caps the power in dBm. 0, the default, leaves it at the regulatory limit.
  - On the bench a few dBm is plenty. At full power the receiver saturates, RSSI reads 0 and the rate drops.
  - Two boards flooding at full power tripped the bench supply's current limit once.
- `--flood` or `--no-flood` overrides which side floods, and `--rate` paces the flood in kbps. 0 floods as fast as the link takes it.
- Every build compiles morselib at `-O2`. At `-Os`, the normal firmware's level, the same boards carried 4.5 Mbps instead of 10 to 11. The cause is not found yet.
- Each build goes to `fw/mradio/build/link_test_<role>`, with the settings it flashed in `link_test.json`. Captures on the same PC copy those settings into the report header.

## In the field

1. Flash both boards the night before, with the same sub-band and width.
2. Run `./harness.py` on the laptop at each end and give the run a name and a length.
3. In the car, press Enter on arriving at a stop. That marks `stop 1`, `stop 2` and so on. Type `drive` and Enter when leaving, so the drive between stops is its own segment.

- Run a GPS logger on a phone in the car. `windows.csv` has a timestamp on every row, so positions join afterward by time.
- The car laptop only sees what its board sends. Loss and received throughput come from the stationary end, so keep both runs.

## Output

Each run is a folder under `results/` named by start time and test name.

- `capture.log` is every console line and stop mark, prefixed with Unix time and the console number.
- `meta.json` has the start time, the test name and the last flash settings from this PC.
- `windows.csv` is one row per 5 s window, with the stop it fell in.
- `report.html` has the stop table, charts with the stop marks and link drops, the event list and every window.
