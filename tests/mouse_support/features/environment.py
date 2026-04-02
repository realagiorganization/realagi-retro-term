from __future__ import annotations

import os
import shutil
import signal
import subprocess
import time
from pathlib import Path


def _slugify(value: str) -> str:
    parts = ["".join(ch.lower() if ch.isalnum() else "-" for ch in value)]
    slug = "".join(parts).strip("-")
    while "--" in slug:
        slug = slug.replace("--", "-")
    return slug or "scenario"


def _terminate_process(process: subprocess.Popen[str] | None) -> None:
    if process is None or process.poll() is not None:
        return

    try:
        os.killpg(process.pid, signal.SIGTERM)
    except ProcessLookupError:
        return

    deadline = time.time() + 5
    while time.time() < deadline:
        if process.poll() is not None:
            return
        time.sleep(0.1)

    try:
        os.killpg(process.pid, signal.SIGKILL)
    except ProcessLookupError:
        return


def before_all(context) -> None:
    userdata = context.config.userdata
    context.repo_root = Path(userdata["repo_root"]).resolve()
    context.artifacts_root = Path(userdata["artifacts_dir"]).resolve()
    context.app_binary = Path(userdata["app_binary"]).resolve()
    context.artifacts_root.mkdir(parents=True, exist_ok=True)

    if "DISPLAY" not in os.environ:
        raise RuntimeError("DISPLAY must be set for the mouse BDD suite")


def before_scenario(context, scenario) -> None:
    scenario_dir = context.artifacts_root / _slugify(scenario.name)
    if scenario_dir.exists():
        shutil.rmtree(scenario_dir)
    scenario_dir.mkdir(parents=True, exist_ok=True)

    context.scenario_dir = scenario_dir
    context.app_process = None
    context.app_log_handle = None
    context.desktop = None


def after_scenario(context, scenario) -> None:
    if getattr(context, "scenario_dir", None):
        screenshot_path = context.scenario_dir / "final-screen.png"
        if shutil.which("scrot"):
            subprocess.run(["scrot", str(screenshot_path)], check=False)

        with (context.scenario_dir / "xwininfo.txt").open("w", encoding="utf-8") as handle:
            subprocess.run(["xwininfo", "-root", "-tree"], stdout=handle, stderr=subprocess.STDOUT, check=False)

    _terminate_process(getattr(context, "app_process", None))

    if getattr(context, "app_log_handle", None) is not None:
        context.app_log_handle.close()
