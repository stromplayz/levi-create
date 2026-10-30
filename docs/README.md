# LeviLamina Developer Guide — Knowledge Base

This directory is the distilled, verified digest of **every page of the official LeviLamina developer documentation** (docs.levimc.org), extracted page-by-page and cross-checked against the canonical markdown sources in the `LiteLDev/LeviLamina` repository. It exists so that AI coding agents (see [`../AGENTS.md`](../AGENTS.md)) and humans can work in this repo without leaving it.

> Extracted: **October 2026** — from docs.levimc.org + `LiteLDev/LeviLamina@main → docs/main/contents/`.

## Index

| # | File | Covers (source pages) |
|---|---|---|
| 01 | [Quickstart — Create Your First Mod](01-quickstart-create-your-first-mod.md) | The official tutorial end-to-end (workspace, lifecycle, command, config, KVDB, events, form, run, publish) |
| 02 | [Build & Toolchain](02-build-and-toolchain.md) | Build Guide + CI/CD + version injection |
| 03 | [Commands](03-commands.md) | Command how-to guide + Command API reference |
| 04 | [Events](04-events.md) | Event how-to guide + Event API reference (all built-in events) |
| 05 | [Forms (legacy `ll::form`)](05-forms.md) | Form how-to guide + Form API reference |
| 06 | [Data-driven UI (`ll::ui`)](06-data-driven-ui.md) | DDUI how-to guide + DDUI API reference |
| 07 | [Config & Reflection](07-config-and-reflection.md) | Config API + Reflection + Reflection (Advanced) |
| 08 | [Storage & Data](08-storage-and-data.md) | Data API (KeyValueDB, Observable, Version, VersionRequirement) |
| 09 | [Hooks & Memory](09-hooks-and-memory.md) | Hook guide, Memory & Hook API, Find Function guide, Interface Export guide |
| 10 | [Coroutines, Threads & Chrono](10-coroutines-threads-chrono.md) | Coroutine guide + Thread API + Chrono API |
| 11 | [I18n & Logging](11-i18n-and-logging.md) | I18n guide + I18n API + IO & Logger API |
| 12 | [Networking](12-networking.md) | Network API (custom packets, SimpleTunnel example) |
| 13 | [Services & Bedrock Access](13-services-and-bedrock.md) | Service API (ServiceManager, Bedrock accessors) |
| 14 | [Mod Lifecycle & Manifest](14-mod-lifecycle-and-manifest.md) | Mod API reference (Mod, NativeMod, Manifest, directories) |
| 15 | [Client Input & Android Native Mods](15-client-input-and-android.md) | Input (Client) API + the 10 Android Preloader-SDK pages |
| 16 | [Architecture & FAQ](16-architecture-and-faq.md) | Architecture page + Developer FAQ |
| 17 | [Core Utilities](17-core-utilities.md) | Base, Expected, Utils, Thread executors cross-cutting notes |
| — | [Sources](sources.md) | Every source URL + URL corrections discovered while mining |

## Which file do I need?

- "How do I make the mod do X on a player?" → `04-events.md` + `03-commands.md`
- "How do I show a menu/dialog?" → `05-forms.md` (static) or `06-data-driven-ui.md` (reactive)
- "Where do I store data?" → `08-storage-and-data.md`
- "How do I change settings?" → `07-config-and-reflection.md`
- "How do I modify game behavior at binary level?" → `09-hooks-and-memory.md`
- "Why did the server crash with ACCESS_VIOLATION?" → `16-architecture-and-faq.md` (thread rules)
