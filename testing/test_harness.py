#!/usr/bin/env python3
import json
import tempfile
from dataclasses import asdict
from datetime import datetime
from pathlib import Path

import harness

T0 = 1_000_000.0


def window_line(role: str, tx: int, rx: int, pkts: int, lost: int, ping: str, rtt: int, rssi: str, rate: str) -> str:
    return (
        f"link test {role}: t=1s tx {tx} kbps rx {rx} kbps pkts {pkts} lost {lost} ooo 0 ping {ping} avg {rtt} ms "
        + f"rssi {rssi} rate {rate} att 10 ok 9 stall 0 err 0"
    )


def main() -> None:
    entries = [
        (T0 + 1, "0", "mradio, role ap, built Nov 21 2026 10:00:00"),
        (T0 + 2, "1", "halow: link up, ip 192.168.2.2"),
        (T0 + 5, "0", window_line("ap", 0, 8000, 690, 10, "5/5", 7, "na", "na")),
        (T0 + 5, "1", window_line("sta", 8100, 0, 0, 0, "5/5", 6, "-60", "mcs7 8MHz sgi")),
        (T0 + 6, "mark", "stop 1"),
        (T0 + 10, "0", window_line("ap", 0, 4000, 300, 0, "4/5", 20, "na", "na")),
        (T0 + 10, "1", window_line("sta", 4100, 0, 0, 0, "4/5", 19, "0", "mcs3 8MHz lgi")),
        (T0 + 11, "1", "halow: link down, ip 192.168.2.2"),
        (T0 + 12, "mark", "drive"),
        (T0 + 15, "1", window_line("sta", 0, 0, 0, 0, "0/5", 0, "na", "na")),
        (T0 + 16, "1", harness.CONSOLE_LOST_MARKER),
    ]
    parsed = harness.parse_capture(entries, T0)
    assert len(parsed.windows) == 5
    first_sta = parsed.windows[1]
    assert first_sta.role == "sta" and first_sta.rssi_dbm == -60 and first_sta.mcs == 7 and first_sta.rate_bw_mhz == 8
    assert parsed.windows[0].rssi_dbm is None and parsed.windows[0].mcs is None
    assert parsed.windows[0].loss_pct is not None and abs(parsed.windows[0].loss_pct - 100 * 10 / 700) < 1e-9
    # a saturated receiver reports rssi 0, which must not count as a reading
    assert parsed.windows[3].rssi_dbm is None
    assert parsed.windows[4].rtt_ms is None and parsed.windows[4].segment == "drive"
    assert [mark.text for mark in parsed.marks] == ["stop 1", "drive"]
    kinds = [(event.board, event.kind) for event in parsed.events]
    assert kinds == [("ap", "boot"), ("console 1", "link up"), ("sta", "link down"), ("sta", "console lost")], kinds

    rows = harness.segments(parsed, captured_s=20)
    labels = [(row.label, row.role) for row in rows]
    assert labels == [("start", "ap"), ("start", "sta"), ("stop 1", "ap"), ("stop 1", "sta"), ("drive", "sta")], labels
    stop_ap = rows[2]
    assert stop_ap.rx_mbps == 4.0 and stop_ap.loss_pct == 0 and (stop_ap.ping_recv, stop_ap.ping_total) == (4, 5)
    assert rows[3].top_mcs == 3 and rows[3].rssi_dbm is None
    assert rows[4].tx_mbps == 0 and rows[4].rtt_ms is None

    empty = harness.segments(harness.parse_capture([], T0), captured_s=0)
    assert empty == []

    started = datetime(2026, 11, 21, 9, 5, 7)
    assert harness.run_dir_name(started, "Field 2, car / mast") == "2026-11-21_090507_field_2_car_mast"
    assert harness.run_dir_name(started, "") == "2026-11-21_090507"

    settings = harness.FlashSettings("sta", "low", 8, 0, True, 0, "2026-11-21T09:00:00-05:00")
    assert harness.flash_settings_from(asdict(settings)) == settings
    assert harness.flash_settings_from({**asdict(settings), "bw_mhz": True}) is None
    assert harness.flash_settings_from({**asdict(settings), "flood": "yes"}) is None
    assert harness.flash_settings_from([]) is None

    with tempfile.TemporaryDirectory() as temp:
        run_dir = Path(temp)
        meta = harness.RunMeta("2026-11-21T09:05:07-05:00", T0, 20, "</script>", ["/dev/null"], False, [settings])
        _ = (run_dir / "meta.json").write_text(json.dumps(asdict(meta)))
        _ = (run_dir / "capture.log").write_text("".join(f"{epoch:.3f} {source} {text}\n" for epoch, source, text in entries))
        live_path, _ = harness.write_report(run_dir, live=True)
        assert harness.LIVE_REFRESH_TAG in live_path.read_text()
        report_path, report_rows = harness.write_report(run_dir)
        page = report_path.read_text()
        assert harness.LIVE_REFRESH_TAG not in page and "__REFRESH__" not in page
        assert not (run_dir / "report.html.partial").exists()
        assert report_rows == rows
        assert page.count("</script>") == 2, "the name must not close the data script early"
        csv_lines = (run_dir / "windows.csv").read_text().splitlines()
        assert len(csv_lines) == 6 and csv_lines[0].startswith("utc,t,role,segment")
    print("ok")


if __name__ == "__main__":
    main()
