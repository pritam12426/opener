#!/usr/bin/env python3

from os import environ
from pathlib import Path
from tomllib import load

commands_group: dict[str, list[dict]] = dict()

XDG_CONFIG_HOME: Path = Path(environ["XDG_CONFIG_HOME"] or "~/.config").expanduser()
CONFIG_FILE: Path = XDG_CONFIG_HOME / "openr" / "config.toml"

with CONFIG_FILE.open("rb") as f:
	config_data: dict = load(f)

commands_group = config_data["defaults"]
ext_map = config_data["extension"]
mimetype = config_data["mimetype"]

ext_hash: dict[str, list[dict]] = {}

for extn, group_name in ext_map.items():
	if isinstance(group_name, str) and group_name in commands_group:
		ext_hash[extn] = commands_group[group_name]

file = Path("some.mp4")
exten = file.suffix.lower().removeprefix(".")

if exten not in ext_hash:
	raise RuntimeError("No handler for extension")

for cmd in ext_hash[exten]:
	fork: str = "[fork]" if cmd.get("fork") else ""
	silent: str = "[silent]" if cmd.get("silent") else ""
	pager: str = "[pager]" if cmd.get("pager") else ""

	print(cmd["command"], end=" ")
	print(f"{fork} {silent} {pager}")
