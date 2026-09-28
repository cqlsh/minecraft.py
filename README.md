<h1 align="center">minecraft.py</h1>

<p align="center">
  <a href="https://www.python.org/downloads/"><img
    src="https://img.shields.io/badge/python-3.14.7%2B-3776AB?style=for-the-badge&logo=python&logoColor=white"
    alt="Python 3.14.7 or higher"></a>
  <a href="#supported-platforms"><img
    src="https://raw.githubusercontent.com/cqlsh/minecraft.py/main/.github/badges/minecraft.svg"
    alt="Java and Bedrock Edition"></a>
  <a href="https://github.com/cqlsh/minecraft.py/blob/main/LICENSE"><img
    src="https://img.shields.io/badge/license-MIT-blue?style=for-the-badge&logo=opensourceinitiative&logoColor=white"
    alt="MIT license"></a>
</p>

A modern, fast and fully typed framework for writing Minecraft server plugins in Python, for Java and Bedrock Edition.

## Key Features

- Modern Pythonic API using `async` and `await`.
- The Bukkit API you already know, in Python.
- One plugin for every server, Java and Bedrock Edition.
- Optimised in both speed and memory.

## Installing

**Python 3.14.7 or higher is required**

> [!NOTE]
> minecraft.py is in early development and not usable yet. The examples below show the target design.

To install the development version, do the following:

```sh
$ git clone https://github.com/cqlsh/minecraft.py
$ cd minecraft.py
$ python3 -m pip install -U .
```

Please note that a C++20 compiler is required to build the library, for example Visual Studio 2022, GCC or Clang.

### Supported Platforms

Support for the following server software is planned:

- Java Edition: Paper, Folia, Purpur, Spigot, Fabric, NeoForge, Sponge, Velocity and BungeeCord
- Bedrock Edition: Bedrock Dedicated Server (via Endstone or LeviLamina), Nukkit, AllayMC and PocketMine-MP

## Quick Example

```py
from minecraft.event import Listener, event_handler
from minecraft.event.player import PlayerJoinEvent
from minecraft.plugin.python import PythonPlugin

class JoinListener(Listener):
    __slots__ = ()

    @event_handler()
    async def on_player_join(self, event: PlayerJoinEvent) -> None:
        player = event.player
        await player.teleport_async(player.world.spawn_location)
        player.send_message(f'Welcome, {player.name}!')

class Greeter(PythonPlugin):
    def on_enable(self) -> None:
        self.server.plugin_manager.register_events(JoinListener(), self)
```

### Command Example

```py
from minecraft.command import Command, CommandSender
from minecraft.entity import Player
from minecraft.plugin.python import PythonPlugin

class Healer(PythonPlugin):
    def on_command(self, sender: CommandSender, command: Command, label: str, args: list[str]) -> bool:
        if not isinstance(sender, Player):
            sender.send_message('Only players can be healed.')
            return True

        sender.health = 20.0
        sender.send_message('You have been healed.')
        return True
```

Every plugin is described by a `plugin.yml`, just like a Java plugin. Build it with `python -m minecraft build`
and put the resulting `.pyz` file into the `plugins` folder of your server.

## Links

- [Issue tracker](https://github.com/cqlsh/minecraft.py/issues)
- [Paper API reference](https://jd.papermc.io/paper/)