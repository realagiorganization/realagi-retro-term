from __future__ import annotations

import os
import re
import signal
import subprocess
import time
from dataclasses import dataclass
from pathlib import Path

from behave import given, then, when
from RPA.Desktop import Desktop


@dataclass
class WindowGeometry:
    window_id: str
    x: int
    y: int
    width: int
    height: int


def _desktop(context) -> Desktop:
    if context.desktop is None:
        context.desktop = Desktop()
    return context.desktop


def _wait_for_path(path: Path, timeout_seconds: float) -> Path:
    deadline = time.time() + timeout_seconds
    while time.time() < deadline:
        if path.exists():
            return path
        time.sleep(0.1)
    raise AssertionError(f"Timed out waiting for {path}")


def _read_text(path: Path) -> str:
    return path.read_text(encoding="utf-8").strip()


def _parse_terminal_size(path: Path) -> tuple[int, int]:
    rows_text, cols_text = _read_text(path).split()
    return int(rows_text), int(cols_text)


def _parse_position(value: str) -> tuple[int, int]:
    match = re.fullmatch(r"\+(-?\d+)\+(-?\d+)", value)
    if not match:
        raise AssertionError(f"Could not parse window position from {value!r}")
    return int(match.group(1)), int(match.group(2))


def _parse_size(value: str) -> tuple[int, int]:
    match = re.fullmatch(r"(\d+)x(\d+)\+-?\d+\+-?\d+", value)
    if not match:
        raise AssertionError(f"Could not parse window size from {value!r}")
    return int(match.group(1)), int(match.group(2))


def _find_main_window(timeout_seconds: float) -> WindowGeometry:
    deadline = time.time() + timeout_seconds
    while time.time() < deadline:
        output = subprocess.check_output(["xwininfo", "-root", "-tree"], text=True)
        best: WindowGeometry | None = None

        for raw_line in output.splitlines():
            line = raw_line.strip()
            if not line.startswith("0x") or '"' not in line:
                continue
            if "Qt Selection Owner" in line:
                continue

            parts = line.rsplit(maxsplit=2)
            if len(parts) != 3:
                continue

            prefix, relative_geometry, absolute_geometry = parts
            window_id = prefix.split()[0]
            width, height = _parse_size(relative_geometry)
            if width < 200 or height < 200:
                continue

            x, y = _parse_position(absolute_geometry)
            candidate = WindowGeometry(window_id=window_id, x=x, y=y, width=width, height=height)
            if best is None or candidate.width * candidate.height > best.width * best.height:
                best = candidate

        if best is not None:
            return best
        time.sleep(0.1)

    raise AssertionError("Timed out waiting for the retro term window to appear")


def _cell_center(context, column: float, row: float) -> tuple[int, int]:
    rows, cols = context.terminal_size
    geometry: WindowGeometry = context.window_geometry
    x = geometry.x + int(round((column + 0.5) * geometry.width / cols))
    y = geometry.y + int(round((row + 0.5) * geometry.height / rows))
    return x, y


def _click_point(context, x: int, y: int) -> None:
    _desktop(context).click(f"point:{x},{y}")
    time.sleep(0.2)


def _launch_fixture(context, fixture_name: str) -> None:
    fixture_path = context.repo_root / "tests" / "mouse_support" / "fixtures" / fixture_name
    if not fixture_path.exists():
        raise AssertionError(f"Missing fixture script {fixture_path}")

    app_log_path = context.scenario_dir / "app.log"
    context.app_log_handle = app_log_path.open("w", encoding="utf-8")

    env = os.environ.copy()
    env.update(
        {
            "REALAGI_RETRO_TERM_DISABLE_SINGLE_INSTANCE": "1",
            "QT_QPA_PLATFORM": "xcb",
            "QT_QUICK_BACKEND": "software",
            "LIBGL_ALWAYS_SOFTWARE": "1",
            "TERM": env.get("TERM", "xterm-256color"),
        }
    )

    command = [
        str(context.app_binary),
        "--default-settings",
        "--profile",
        "Boring",
        "-e",
        "bash",
        str(fixture_path),
        str(context.scenario_dir),
    ]

    context.app_process = subprocess.Popen(
        command,
        cwd=context.repo_root,
        env=env,
        stdout=context.app_log_handle,
        stderr=subprocess.STDOUT,
        start_new_session=True,
        text=True,
    )

    _wait_for_path(context.scenario_dir / "terminal-size.txt", timeout_seconds=20)
    context.terminal_size = _parse_terminal_size(context.scenario_dir / "terminal-size.txt")
    context.window_geometry = _find_main_window(timeout_seconds=20)

    if context.app_process.poll() is not None:
        raise AssertionError((context.scenario_dir / "app.log").read_text(encoding="utf-8"))


def _tmux_panes(context) -> dict[str, dict[str, int | str]]:
    panes_path = _wait_for_path(context.scenario_dir / "tmux-panes.txt", timeout_seconds=10)
    panes: dict[str, dict[str, int | str]] = {}
    lines = _read_text(panes_path).splitlines()
    if len(lines) < 2:
        raise AssertionError(f"Expected at least two tmux panes, got: {lines!r}")

    ordered = []
    for line in lines:
        pane_id, left, top, width, height = line.split()
        pane = {
            "id": pane_id,
            "left": int(left),
            "top": int(top),
            "width": int(width),
            "height": int(height),
        }
        ordered.append(pane)

    ordered.sort(key=lambda pane: int(pane["top"]))
    panes["top"] = ordered[0]
    panes["bottom"] = ordered[-1]
    return panes


def _active_tmux_pane_id(context) -> str:
    active_path = _wait_for_path(context.scenario_dir / "tmux-active-pane.txt", timeout_seconds=10)
    for line in _read_text(active_path).splitlines():
        active_flag, pane_id = line.split()
        if active_flag == "1":
            return pane_id
    raise AssertionError(f"No active pane found in {active_path}")


def _mc_button_column(context, button_index: int) -> int:
    _, cols = context.terminal_size
    return int((button_index + 0.5) * cols / 10)


@given('the retro term app is running the "{fixture_name}" fixture')
def step_start_fixture(context, fixture_name: str) -> None:
    _launch_fixture(context, fixture_name)


@when('I click inside the tmux pane named "{pane_name}"')
def step_click_tmux_pane(context, pane_name: str) -> None:
    pane = _tmux_panes(context)[pane_name]
    column = int(pane["left"]) + int(pane["width"]) // 2
    row = int(pane["top"]) + int(pane["height"]) // 2
    x, y = _cell_center(context, column, row)
    _click_point(context, x, y)


@when("I press the tmux active-pane capture hotkey")
def step_press_tmux_hotkey(context) -> None:
    _desktop(context).press_keys("F12")
    time.sleep(0.2)


@then('the tmux active pane artifact should equal the "{pane_name}" pane id')
def step_assert_tmux_active_pane(context, pane_name: str) -> None:
    expected = str(_tmux_panes(context)[pane_name]["id"])
    actual = _active_tmux_pane_id(context)
    if actual != expected:
        raise AssertionError(f"Expected active tmux pane {expected}, got {actual}")


@when('I click the mc command button "{button_name}"')
def step_click_mc_button(context, button_name: str) -> None:
    button_indexes = {
        "help": 0,
        "menu": 1,
        "view": 2,
        "edit": 3,
        "copy": 4,
        "renmov": 5,
        "mkdir": 6,
        "delete": 7,
        "pulldn": 8,
        "quit": 9,
    }
    try:
        button_index = button_indexes[button_name.lower()]
    except KeyError as exc:
        raise AssertionError(f"Unknown mc button {button_name}") from exc

    rows, _ = context.terminal_size
    x, y = _cell_center(context, _mc_button_column(context, button_index), rows - 1)
    _click_point(context, x, y)


@when('I type "{text}" and press enter')
def step_type_text_and_enter(context, text: str) -> None:
    _desktop(context).type_text(text)
    time.sleep(0.2)
    _desktop(context).press_keys("ENTER")
    time.sleep(0.2)


@when('I press the key "{key_name}"')
def step_press_named_key(context, key_name: str) -> None:
    _desktop(context).press_keys(key_name.upper())
    time.sleep(0.2)


@then('the directory "{directory_name}" should exist inside the mc fixture root')
def step_assert_mc_directory(context, directory_name: str) -> None:
    root_path = Path(_read_text(_wait_for_path(context.scenario_dir / "mc-root-path.txt", timeout_seconds=5)))
    expected_path = root_path / directory_name
    _wait_for_path(expected_path, timeout_seconds=10)
