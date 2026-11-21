#!/usr/bin/env python3
"""HaLow link test harness: flash a board with the link test, then capture the consoles into a live report."""

from __future__ import annotations

import argparse
import csv
import glob
import html
import json
import os
import re
import select
import subprocess
import sys
import termios
import threading
import time
import tty
import webbrowser
from collections.abc import Callable
from dataclasses import asdict, dataclass, field
from datetime import datetime
from pathlib import Path
from typing import TextIO, cast

ROOT = Path(__file__).resolve().parent
REPO = ROOT.parent
PROJECT = REPO / "fw" / "mradio"
REPORT_TEMPLATE_PATH = ROOT / "report_template.html"
RESULTS_DIR = ROOT / "results"
ROLES = ("ap", "sta")
SUB_BANDS = ("low", "mid", "high")
WIDTHS_MHZ = (1, 2, 4, 8)
CONSOLE_GLOB = "/dev/serial/by-id/usb-STMicroelectronics_STLINK*"
CONSOLE_LOST_MARKER = "#harness: console lost"
MARK_SOURCE = "mark"
FLASH_RECORD_NAME = "link_test.json"
# morselib at -Os or -O1 gave 4.5 to 5 Mbps on the bench and -O2 gave 10 to 11, the cause is not found yet
MORSELIB_OPT = "-O2"
STALE_DRAIN_S = 1.0
LIVE_REFRESH_S = 5.0
DEFAULT_MINUTES = 1.0
COMMANDS = ("flash", "capture", "report")
LIVE_REFRESH_TAG = f'<meta http-equiv="refresh" content="{LIVE_REFRESH_S:g}">'

WINDOW_PATTERN = re.compile(
    r"link test (ap|sta): t=(\d+)s tx (\d+) kbps rx (\d+) kbps pkts (\d+) lost (\d+) ooo (\d+) "
    + r"ping (\d+)/(\d+) avg (\d+) ms rssi (na|-?\d+) rate (?:na|mcs(\d+) (\d+)MHz (sgi|lgi)) "
    + r"att (\d+) ok (\d+) stall (\d+) err (\d+)"
)
# older images printed "mradio halow demo", which past runs still contain
BANNER_PATTERN = re.compile(r"mradio(?: halow demo)?, role (ap|sta)")


class HarnessError(Exception):
    pass


@dataclass
class FlashSettings:
    role: str
    sub_band: str
    bw_mhz: int
    tx_power_dbm: int
    flood: bool
    rate_kbps: int
    flashed: str


@dataclass
class RunMeta:
    started: str
    started_epoch: float
    captured_s: float
    name: str
    consoles: list[str]
    reset: bool
    flashes: list[FlashSettings]


@dataclass
class Window:
    t: float
    epoch: float
    role: str
    segment: str
    uptime_s: int
    tx_kbps: int
    rx_kbps: int
    pkts: int
    lost: int
    ooo: int
    ping_recv: int
    ping_total: int
    rtt_ms: int | None
    rssi_dbm: int | None
    mcs: int | None
    rate_bw_mhz: int | None
    attempts: int
    acked: int
    stalls: int
    errors: int
    loss_pct: float | None


@dataclass
class Event:
    t: float
    board: str
    kind: str
    text: str


@dataclass
class Segment:
    label: str
    start_t: float
    end_t: float
    role: str
    windows: int
    tx_mbps: float | None
    rx_mbps: float | None
    loss_pct: float | None
    ping_recv: int
    ping_total: int
    rtt_ms: float | None
    rssi_dbm: float | None
    top_mcs: int | None


@dataclass
class ParsedRun:
    windows: list[Window] = field(default_factory=lambda: list[Window]())
    events: list[Event] = field(default_factory=lambda: list[Event]())
    marks: list[Event] = field(default_factory=lambda: list[Event]())


@dataclass
class Console:
    index: int
    path: str
    fd: int
    pending: bytes = b""
    lost: bool = False


def run(command: list[str], description: str) -> None:
    if subprocess.run(command, check=False).returncode != 0:
        raise HarnessError(f"{description} failed, see its output above")


def build_dir(role: str) -> Path:
    return PROJECT / "build" / f"link_test_{role}"


def flash(settings: FlashSettings, probe_index: int) -> None:
    directory = build_dir(settings.role)
    run(
        [
            "cmake", "-S", str(PROJECT / "CM7"), "-B", str(directory), "-G", "Ninja",
            f"-DCMAKE_TOOLCHAIN_FILE={PROJECT / 'gcc-arm-none-eabi.cmake'}",
            "-DCMAKE_BUILD_TYPE=Release",
            f"-DHALOW_DEFAULT_ROLE={settings.role}",
            f"-DHALOW_SUB_BAND={settings.sub_band}",
            f"-DHALOW_BW_MHZ={settings.bw_mhz}",
            f"-DHALOW_TX_POWER_DBM={settings.tx_power_dbm}",
            "-DLINK_TEST=ON",
            f"-DMORSELIB_OPT={MORSELIB_OPT}",
            f"-DLINK_TEST_FLOOD={'ON' if settings.flood else 'OFF'}",
            f"-DLINK_TEST_RATE_KBPS={settings.rate_kbps}",
        ],
        "cmake configure",
    )
    run(["cmake", "--build", str(directory)], "build")
    run(
        [
            "STM32_Programmer_CLI", "-c", "port=SWD", f"index={probe_index}", "mode=UR", "reset=HWrst",
            "-w", str(directory / "mradio_CM7.elf"), "-v", "-rst",
        ],
        "flash",
    )
    _ = (directory / FLASH_RECORD_NAME).write_text(json.dumps(asdict(settings), indent=2) + "\n")


def flash_settings_from(raw: object) -> FlashSettings | None:
    if not isinstance(raw, dict):
        return None
    fields = cast(dict[str, object], raw)
    role, sub_band, flashed = fields.get("role"), fields.get("sub_band"), fields.get("flashed")
    bw_mhz, tx_power_dbm, rate_kbps = fields.get("bw_mhz"), fields.get("tx_power_dbm"), fields.get("rate_kbps")
    flood = fields.get("flood")
    if not (isinstance(role, str) and isinstance(sub_band, str) and isinstance(flashed, str)):
        return None
    numbers = (bw_mhz, tx_power_dbm, rate_kbps)
    if any(isinstance(value, bool) or not isinstance(value, int) for value in numbers) or not isinstance(flood, bool):
        return None
    return FlashSettings(
        role=role,
        sub_band=sub_band,
        bw_mhz=cast(int, bw_mhz),
        tx_power_dbm=cast(int, tx_power_dbm),
        flood=flood,
        rate_kbps=cast(int, rate_kbps),
        flashed=flashed,
    )


def load_flash_settings(role: str) -> FlashSettings | None:
    try:
        raw = cast(object, json.loads((build_dir(role) / FLASH_RECORD_NAME).read_text()))
    except (OSError, json.JSONDecodeError):
        return None
    return flash_settings_from(raw)


def open_console(path: str) -> int:
    fd = os.open(path, os.O_RDONLY | os.O_NOCTTY | os.O_NONBLOCK)
    _ = tty.setraw(fd)
    attributes = termios.tcgetattr(fd)
    attributes[4] = termios.B115200
    attributes[5] = termios.B115200
    termios.tcsetattr(fd, termios.TCSANOW, attributes)
    return fd


def reset_probes(count: int, failures: list[str]) -> None:
    for index in range(count):
        result = subprocess.run(
            ["STM32_Programmer_CLI", "-c", "port=SWD", f"index={index}", "mode=UR", "reset=HWrst", "-rst"],
            capture_output=True, text=True, check=False,
        )
        if result.returncode != 0:
            failures.append(f"reset of probe {index} failed:\n{result.stdout.strip()[-400:]}")


class CaptureLog:
    def __init__(self, log: TextIO) -> None:
        self.log: TextIO = log
        self.stops: int = 0

    def write(self, source: str, text: str) -> None:
        now = time.time()
        _ = self.log.write(f"{now:.3f} {source} {text}\n")
        self.log.flush()
        print(f"[{datetime.fromtimestamp(now):%H:%M:%S} {source}] {text}", flush=True)

    def mark(self, typed: str) -> None:
        label = typed.strip()
        if not label:
            self.stops += 1
            label = f"stop {self.stops}"
        self.write(MARK_SOURCE, label)


def feed(console: Console, chunk: bytes, capture_log: CaptureLog) -> None:
    console.pending += chunk
    *complete, console.pending = console.pending.split(b"\n")
    for raw_line in complete:
        text = raw_line.decode("utf-8", errors="replace").strip("\r\x00 ")
        if text:
            capture_log.write(str(console.index), text)


def pump(
    consoles: list[Console], capture_log: CaptureLog, until: float | None, refresh: Callable[[], None]
) -> None:
    stdin_fd = sys.stdin.fileno()
    watch_stdin = True
    refreshed_at = time.monotonic()
    while True:
        if time.monotonic() - refreshed_at >= LIVE_REFRESH_S:
            refresh()
            refreshed_at = time.monotonic()
        active = [console for console in consoles if not console.lost]
        if not active:
            print("every console is gone, stopping", file=sys.stderr)
            return
        timeout = 0.5 if until is None else min(until - time.monotonic(), 0.5)
        if timeout <= 0:
            return
        watched = [console.fd for console in active] + ([stdin_fd] if watch_stdin else [])
        ready, _, _ = select.select(watched, [], [], timeout)
        if watch_stdin and stdin_fd in ready:
            typed = sys.stdin.readline()
            if typed:
                capture_log.mark(typed)
            else:
                # stdin closed, e.g. run from a script, so marks can no longer come in
                watch_stdin = False
        for console in active:
            if console.fd not in ready:
                continue
            try:
                chunk = os.read(console.fd, 4096)
            except BlockingIOError:
                continue
            except OSError:
                chunk = b""
            if chunk:
                feed(console, chunk, capture_log)
            else:
                # the ST-LINK dropping off USB ends its tty
                console.lost = True
                capture_log.write(str(console.index), CONSOLE_LOST_MARKER)


def drain(consoles: list[Console], seconds: float) -> None:
    deadline = time.monotonic() + seconds
    while (remaining := deadline - time.monotonic()) > 0:
        ready, _, _ = select.select([console.fd for console in consoles], [], [], remaining)
        for console in consoles:
            if console.fd in ready:
                try:
                    _ = os.read(console.fd, 4096)
                except OSError:
                    continue


def run_dir_name(started: datetime, name: str) -> str:
    slug = re.sub(r"[^a-z0-9]+", "_", name.lower()).strip("_")[:40]
    return f"{started:%Y-%m-%d_%H%M%S}" + (f"_{slug}" if slug else "")


def capture(minutes: float, name: str, reset: bool, open_report: bool) -> Path:
    paths = sorted(glob.glob(CONSOLE_GLOB))
    if not paths:
        raise HarnessError(f"no ST-LINK console under {CONSOLE_GLOB}, plug in a board")
    started = datetime.now().astimezone()
    run_dir = RESULTS_DIR / run_dir_name(started, name)
    run_dir.mkdir(parents=True, exist_ok=False)
    consoles = [Console(index, path, open_console(path)) for index, path in enumerate(paths)]
    # the ST-LINK replays what the board printed while the port was closed, so drop it
    drain(consoles, STALE_DRAIN_S)
    t0 = time.monotonic()
    flashes = [settings for role in ROLES if (settings := load_flash_settings(role)) is not None]

    def save_meta() -> None:
        meta = RunMeta(
            started=started.isoformat(timespec="seconds"),
            started_epoch=started.timestamp(),
            captured_s=round(time.monotonic() - t0, 1),
            name=name,
            consoles=paths,
            reset=reset,
            flashes=flashes,
        )
        _ = (run_dir / "meta.json").write_text(json.dumps(asdict(meta), indent=2) + "\n")

    def refresh_live_report() -> None:
        save_meta()
        try:
            _ = write_report(run_dir, live=True)
        except HarnessError as error:
            print(f"warning: live report not written, {error}", file=sys.stderr)

    refresh_live_report()
    report_uri = (run_dir / "report.html").as_uri()
    print(f"capturing {len(consoles)} console(s) for {minutes:g} min into {run_dir}")
    print(f"live report: {report_uri}")
    if open_report and not webbrowser.open(report_uri):
        print("warning: no browser opened, open the link above by hand", file=sys.stderr)
    print("press Enter at each stop to mark it, or type a label such as 'drive' then Enter, Ctrl-C ends")
    with (run_dir / "capture.log").open("w") as log:
        capture_log = CaptureLog(log)
        for console in consoles:
            capture_log.write(str(console.index), f"#harness: console {console.path}")
        reset_failures: list[str] = []
        # each reset takes seconds, so it runs beside the reader to keep the boot lines on time
        resetter = threading.Thread(target=reset_probes, args=(len(consoles), reset_failures), daemon=True)
        try:
            if reset:
                resetter.start()
            pump(consoles, capture_log, t0 + minutes * 60, refresh_live_report)
        except KeyboardInterrupt:
            print("\nstopped, writing the report", file=sys.stderr)
        finally:
            if resetter.is_alive():
                resetter.join()
            for console in consoles:
                os.close(console.fd)
        for failure in reset_failures:
            print(f"warning: {failure}", file=sys.stderr)
    save_meta()
    return run_dir


def read_capture(path: Path) -> list[tuple[float, str, str]]:
    entries: list[tuple[float, str, str]] = []
    if not path.exists():
        return entries
    for raw_line in path.read_text(errors="replace").splitlines():
        parts = raw_line.split(" ", 2)
        if len(parts) < 3:
            continue
        try:
            entries.append((float(parts[0]), parts[1], parts[2]))
        except ValueError:
            continue
    return entries


def optional_int(text: str | None) -> int | None:
    return None if text is None or text == "na" else int(text)


def parse_window(epoch: float, t: float, text: str, segment: str) -> Window | None:
    match = WINDOW_PATTERN.search(text)
    if match is None:
        return None
    groups = match.groups()
    role = cast(str, groups[0])
    uptime, tx, rx, pkts, lost, ooo, ping_recv, ping_total, rtt = (int(cast(str, value)) for value in groups[1:10])
    rssi = optional_int(groups[10])
    attempts, acked, stalls, errors = (int(cast(str, value)) for value in groups[14:18])
    return Window(
        t=t,
        epoch=epoch,
        role=role,
        segment=segment,
        uptime_s=uptime,
        tx_kbps=tx,
        rx_kbps=rx,
        pkts=pkts,
        lost=lost,
        ooo=ooo,
        ping_recv=ping_recv,
        ping_total=ping_total,
        rtt_ms=rtt if ping_recv > 0 else None,
        # a saturated receiver reports 0, which is not a reading
        rssi_dbm=rssi if rssi is not None and rssi < 0 else None,
        mcs=optional_int(groups[11]),
        rate_bw_mhz=optional_int(groups[12]),
        attempts=attempts,
        acked=acked,
        stalls=stalls,
        errors=errors,
        loss_pct=100 * lost / (pkts + lost) if pkts + lost > 0 else None,
    )


def classify_event(text: str) -> str | None:
    if text == CONSOLE_LOST_MARKER:
        return "console lost"
    if BANNER_PATTERN.search(text):
        return "boot"
    if text.startswith("halow: link down"):
        return "link down"
    if text.startswith("halow: link up"):
        return "link up"
    if text.startswith("halow: station") and "not authorized" in text:
        return "station left"
    if text.startswith("halow: station") and "authorized" in text:
        return "station joined"
    if "failed" in text and text.startswith(("halow:", "link test:")):
        return "error"
    return None


def parse_capture(entries: list[tuple[float, str, str]], started_epoch: float) -> ParsedRun:
    parsed = ParsedRun()
    roles: dict[str, str] = {}
    segment = "start"
    for epoch, source, text in entries:
        t = epoch - started_epoch
        if source == MARK_SOURCE:
            segment = text
            parsed.marks.append(Event(t=t, board="mark", kind="mark", text=text))
            continue
        window = parse_window(epoch, t, text, segment)
        if window is not None:
            roles[source] = window.role
            parsed.windows.append(window)
            continue
        banner = BANNER_PATTERN.search(text)
        if banner is not None:
            roles[source] = banner.group(1)
        kind = classify_event(text)
        if kind is not None:
            parsed.events.append(Event(t=t, board=roles.get(source, f"console {source}"), kind=kind, text=text))
    return parsed


def mean(values: list[float]) -> float | None:
    return sum(values) / len(values) if values else None


def summarize_windows(label: str, role: str, windows: list[Window], start_t: float, end_t: float) -> Segment:
    pkts = sum(window.pkts for window in windows)
    lost = sum(window.lost for window in windows)
    mcs_counts: dict[int, int] = {}
    for window in windows:
        if window.mcs is not None:
            mcs_counts[window.mcs] = mcs_counts.get(window.mcs, 0) + window.attempts
    return Segment(
        label=label,
        start_t=start_t,
        end_t=end_t,
        role=role,
        windows=len(windows),
        tx_mbps=mean([window.tx_kbps / 1000 for window in windows]),
        rx_mbps=mean([window.rx_kbps / 1000 for window in windows]),
        loss_pct=100 * lost / (pkts + lost) if pkts + lost > 0 else None,
        ping_recv=sum(window.ping_recv for window in windows),
        ping_total=sum(window.ping_total for window in windows),
        rtt_ms=mean([float(window.rtt_ms) for window in windows if window.rtt_ms is not None]),
        rssi_dbm=mean([float(window.rssi_dbm) for window in windows if window.rssi_dbm is not None]),
        top_mcs=max(mcs_counts, key=lambda mcs: mcs_counts[mcs]) if mcs_counts else None,
    )


def segments(parsed: ParsedRun, captured_s: float) -> list[Segment]:
    bounds = [("start", 0.0)] + [(mark.text, mark.t) for mark in parsed.marks]
    result: list[Segment] = []
    for index, (label, start_t) in enumerate(bounds):
        end_t = bounds[index + 1][1] if index + 1 < len(bounds) else max(captured_s, start_t)
        for role in ROLES:
            # a 5 s window only belongs to a segment when it ended inside it
            inside = [window for window in parsed.windows if window.role == role and start_t < window.t <= end_t]
            if inside:
                result.append(summarize_windows(label, role, inside, start_t, end_t))
    return result


def load_meta(run_dir: Path) -> RunMeta:
    meta_path = run_dir / "meta.json"
    try:
        raw = cast(object, json.loads(meta_path.read_text()))
    except (OSError, json.JSONDecodeError) as error:
        raise HarnessError(f"{meta_path} is missing or not valid JSON: {error}") from error
    if not isinstance(raw, dict):
        raise HarnessError(f"{meta_path} is not a JSON object")
    fields = cast(dict[str, object], raw)
    flashes_raw = fields.get("flashes", [])
    flashes: list[FlashSettings] = []
    if isinstance(flashes_raw, list):
        for item in cast(list[object], flashes_raw):
            settings = flash_settings_from(item)
            if settings is not None:
                flashes.append(settings)
    consoles_raw = fields.get("consoles", [])
    try:
        return RunMeta(
            started=str(fields["started"]),
            started_epoch=float(cast(float, fields["started_epoch"])),
            captured_s=float(cast(float, fields["captured_s"])),
            name=str(fields.get("name", "")),
            consoles=[str(item) for item in cast(list[object], consoles_raw)] if isinstance(consoles_raw, list) else [],
            reset=bool(fields.get("reset", False)),
            flashes=flashes,
        )
    except (KeyError, TypeError, ValueError) as error:
        raise HarnessError(f"{meta_path} is missing a field: {error}") from error


def write_csv(path: Path, windows: list[Window]) -> None:
    names = [name for name in Window.__dataclass_fields__ if name != "epoch"]
    with path.open("w", newline="") as file:
        writer = csv.writer(file)
        writer.writerow(["utc", *names])
        for window in windows:
            values = asdict(window)
            utc = datetime.fromtimestamp(window.epoch).astimezone().isoformat(timespec="seconds")
            writer.writerow([utc, *("" if values[name] is None else values[name] for name in names)])


def write_report(run_dir: Path, live: bool = False) -> tuple[Path, list[Segment]]:
    meta = load_meta(run_dir)
    parsed = parse_capture(read_capture(run_dir / "capture.log"), meta.started_epoch)
    segment_rows = segments(parsed, meta.captured_s)
    write_csv(run_dir / "windows.csv", parsed.windows)
    payload = {
        "meta": asdict(meta),
        "windows": [asdict(window) for window in parsed.windows],
        "events": [asdict(event) for event in parsed.events],
        "marks": [asdict(mark) for mark in parsed.marks],
        "segments": [asdict(segment) for segment in segment_rows],
    }
    if not REPORT_TEMPLATE_PATH.exists():
        raise HarnessError(f"report template not found at {REPORT_TEMPLATE_PATH}")
    title = f"Link test {meta.started[:16].replace('T', ' ')}" + (f", {meta.name}" if meta.name else "")
    embedded = json.dumps(payload).replace("</", "<\\/")
    page = (
        REPORT_TEMPLATE_PATH.read_text()
        .replace("__REFRESH__", LIVE_REFRESH_TAG if live else "")
        .replace("__TITLE__", html.escape(title + (", live" if live else "")))
        .replace("__RUN_DATA__", embedded)
    )
    report_path = run_dir / "report.html"
    # a browser reloading mid-write would show a half page, so swap the file in whole
    partial_path = run_dir / "report.html.partial"
    _ = partial_path.write_text(page)
    _ = partial_path.replace(report_path)
    return report_path, segment_rows


def format_optional(value: float | None, digits: int, unit: str) -> str:
    return "n/a" if value is None else f"{value:.{digits}f}{unit}"


def segment_line(segment: Segment) -> str:
    return (
        f"{segment.label:<12} {segment.role:<3} {segment.windows:>3} windows  "
        f"tx {format_optional(segment.tx_mbps, 2, ' Mbps'):>10}  rx {format_optional(segment.rx_mbps, 2, ' Mbps'):>10}  "
        f"loss {format_optional(segment.loss_pct, 1, '%'):>6}  ping {segment.ping_recv}/{segment.ping_total}  "
        f"rtt {format_optional(segment.rtt_ms, 0, ' ms'):>7}  rssi {format_optional(segment.rssi_dbm, 0, ' dBm'):>8}  "
        f"mcs {'n/a' if segment.top_mcs is None else segment.top_mcs}"
    )


def ask(prompt: str) -> str:
    try:
        return input(prompt).strip()
    except EOFError as error:
        raise HarnessError("input closed before the run was configured") from error


def ask_name() -> str:
    while True:
        name = ask("Test name, e.g. site 1 car mast: ")
        if name:
            return name
        print("  the name labels the run folder and the report, so it cannot be empty")


def ask_minutes() -> float:
    while True:
        answer = ask(f"Minutes to capture [{DEFAULT_MINUTES:g}]: ")
        try:
            minutes = float(answer) if answer else DEFAULT_MINUTES
        except ValueError:
            minutes = 0.0
        if minutes > 0:
            return minutes
        print("  enter a positive number of minutes, e.g. 0.5 or 30")


def main(argv: list[str]) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    commands = parser.add_subparsers(dest="command", required=True)

    flash_parser = commands.add_parser("flash", help="build the link test for one role and flash it")
    _ = flash_parser.add_argument("role", choices=ROLES)
    _ = flash_parser.add_argument("--index", type=int, default=0, help="ST-LINK probe index, default the first one")
    _ = flash_parser.add_argument("--sub-band", choices=SUB_BANDS, default="mid")
    _ = flash_parser.add_argument("--bw", type=int, choices=WIDTHS_MHZ, default=8, help="channel width in MHz")
    _ = flash_parser.add_argument("--tx-power", type=int, default=0, help="max TX power in dBm, 0 for the limit")
    _ = flash_parser.add_argument("--flood", action=argparse.BooleanOptionalAction, default=None,
                                  help="flood UDP at the peer, default on for the sta and off for the ap")
    _ = flash_parser.add_argument("--rate", type=int, default=0, help="flood rate in kbps, 0 for unpaced")

    capture_parser = commands.add_parser("capture", help="log every plugged-in console, the default command")
    _ = capture_parser.add_argument("--name", default=None, help="test name, asked for when not given")
    _ = capture_parser.add_argument("--minutes", type=float, default=None,
                                    help=f"capture length, asked for when not given, default {DEFAULT_MINUTES:g}")
    _ = capture_parser.add_argument("--reset", action="store_true", help="reset the local boards when capture starts")
    _ = capture_parser.add_argument("--no-open", action="store_true", help="print the report link without opening it")

    report_parser = commands.add_parser("report", help="rebuild the report of a past run")
    _ = report_parser.add_argument("run_dir", type=Path)

    # a bare run or one with only options is a capture
    if not argv or (argv[0] not in COMMANDS and argv[0] not in ("-h", "--help")):
        argv = ["capture", *argv]
    arguments = parser.parse_args(argv)
    command = cast(str, arguments.command)
    try:
        if command == "flash":
            role = cast(str, arguments.role)
            tx_power = cast(int, arguments.tx_power)
            rate = cast(int, arguments.rate)
            if not 0 <= tx_power <= 30:
                raise HarnessError("--tx-power must be 0 to 30 dBm")
            if rate < 0:
                raise HarnessError("--rate must not be negative")
            flood_choice = cast(bool | None, arguments.flood)
            settings = FlashSettings(
                role=role,
                sub_band=cast(str, arguments.sub_band),
                bw_mhz=cast(int, arguments.bw),
                tx_power_dbm=tx_power,
                flood=role == "sta" if flood_choice is None else flood_choice,
                rate_kbps=rate,
                flashed=datetime.now().astimezone().isoformat(timespec="seconds"),
            )
            flash(settings, cast(int, arguments.index))
            print(f"flashed {role}: {settings}")
            return 0
        if command == "capture":
            given_name = cast(str | None, arguments.name)
            name = given_name.strip() if given_name is not None and given_name.strip() else ask_name()
            given_minutes = cast(float | None, arguments.minutes)
            if given_minutes is not None and given_minutes <= 0:
                raise HarnessError("--minutes must be positive")
            minutes = given_minutes if given_minutes is not None else ask_minutes()
            run_dir = capture(minutes, name, cast(bool, arguments.reset), not cast(bool, arguments.no_open))
        else:
            run_dir = cast(Path, arguments.run_dir).resolve()
        report_path, segment_rows = write_report(run_dir)
    except HarnessError as error:
        print(f"error: {error}", file=sys.stderr)
        return 1
    except KeyboardInterrupt:
        print("\ncancelled", file=sys.stderr)
        return 130
    for segment in segment_rows:
        print(segment_line(segment))
    print(f"report: {report_path.as_uri()}")
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
