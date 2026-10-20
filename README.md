# vip_smoke (Plugify)

[VIP] Colored Smoke: colours the smoke grenades of VIPs whose 'smoke_color' feature holds an "R G B" value (or 'random'), by writing the projectile's m_vSmokeColor when it spawns. Ported from the Pisex VIP core module VIP_Smoke (vip_smoke). Requires the vip core.

Plugify port (fork) of **VIP_Smoke ([VIP] Colored Smoke)** by Pisex (Metamod (C++)).
Original: https://github.com/Pisex/cs2-vip-modules
The code was rewritten for Plugify / s2sdk; behaviour follows the original.

## Requirements
- [Plugify](https://github.com/untrustedmodders/plugify) with [s2sdk](https://github.com/untrustedmodders/plugify-plugin-s2sdk)
- Plugify dependencies: s2sdk, vip, hudmenu (optional)

## Installation
Download `vip_smoke-1.0.2.zip` from Releases and extract it into `game/csgo/`.

## Building
Requires Docker. `./build.sh` builds in the Steam Runtime 3 (sniper) SDK image; the package
is written to `build/package/vip_smoke`.

`plugins/vip` carry only the API headers of the plugins this one uses.

## License
GPL-3.0, see LICENSE.

