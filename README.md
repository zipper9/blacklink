[![Build Status](https://ci.appveyor.com/api/projects/status/github/zipper9/blacklink?svg=true)](https://ci.appveyor.com/api/projects/status/github/zipper9/blacklink)

BlackLink is an advanced client for the Direct Connect network, initially started as a fork of [FlylinkDC++](https://github.com/pavel-pimenov/flylinkdc-r5xx).

## Features

- Supports TLS 1.3
- Supports IPv4 and IPv6
- Supports StrongDC++'s DHT (Distributed Hash Table)
- CCPM feature (encrypted client-to-client private messages), SUDP (encryption of search results)
- Automatic port forwarding with UPnP or NAT-PMP
- High performance hash database using the [LMDB](https://github.com/LMDB/lmdb) engine
- Allows to use a single port for both TLS and unencrypted TCP connections; only one open TCP and one UDP port is required for full active mode
- Simple built-in web server
- Many bugfixes and improvements in features and UI
- Does not contain ads or "analytics"

## License

BlackLink is free software offered under the terms of the GPLv2 license.

This program uses Fugue Icons by Yusuke Kamiyamane under CC BY 3.0.
https://p.yusukekamiyamane.com

## How to build

BlackLink can be built on Windows with Visual Studio 2019 or Visual Studio 2022 (Full or Community Edition).
Use the provided solution file, blacklink_2019.sln, to build the project. All required dependencies are included in the source tree.
The table below shows the supported OS versions and default build tools. The 64-bit version is recommended.

| Platform      | Build tools | Minimum supported OS version |
|---------------|-------------|------------------------------|
| x86           | v140_xp     | Windows XP                   |
| x86_64        | v142        | Windows 7                    |
| ARMv7 & ARM64 | v142        | Windows 10                   |
