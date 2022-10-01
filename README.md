[![Build Status](https://ci.appveyor.com/api/projects/status/github/zipper9/blacklink?svg=true)](https://ci.appveyor.com/api/projects/status/github/zipper9/blacklink)

BlackLink is an advanced DC++ client, initially started as a fork of [FlylinkDC++](https://github.com/pavel-pimenov/flylinkdc-r5xx).

## Features

- Supports TLS 1.3
- Supports IPv4 and IPv6
- Supports StrongDC++'s DHT (Distributed Hash Table)
- CCPM feature (encrypted client-to-client private messages)
- Automatic port forwarding with UPnP or NAT-PMP
- High performance hash database using the [LMDB](https://github.com/LMDB/lmdb) engine
- Allows to use a single port for both TLS and unencrypted TCP connections; only one open TCP and one UDP port is required for full active mode
- Tons of bugfixes and improvements
- No ads, no preference for specific hubs

This program uses Fugue Icons by Yusuke Kamiyamane.  
https://p.yusukekamiyamane.com

## How to build

BlackLink can be built on Windows with Visual Studio 2019, either full or Community Edition. Use the provided solution, blacklink_2019.sln.
Set the platform to _x64_ and configuration to _Release_.
