# XRTP

xrtp is a tool for RTP packet analysis.

In general, xrtp reads the pcap file and parses the RTP packets (and corresponding RTCP packets) all the way through it, depending on the port selection. xrtp can do timestamp parsing, jitter analysis, payload extraction, etc.

⚠️ xrtp use [Npcap](https://npcap.com), and supports Windows only for now.

## Build

1. Setup CMake.

2. Setup MSVC (2019 or above is preferred).

3. Build

```cmd
    > mkdir build
    > cd build
    > cmake .. -G "<Visual Studio ToolChain>" 
    > cmake --build . --config <Config>
```

## Usage

1. make sure you have installed [Npcap](https://npcap.com), and have wpcap.dll and packet.dll is in your PATH.

2. run xrtp.exe

```cmd
    > xrtp.exe -p <dst port> -d <pt>:<freq>:<type> <file.pcap>
```

3. see more details in `xrtp.exe --help`.

## Note

- port

    port is the destination port of RTP packets.

- description

    `description` define the payload type, frequency (HZ) and payload type.

    use `-d <pt>:<freq>:<type>` to pass description to xrtp.

    once xrtp get a RTP packet, it will check the payload type, if xrtp find a matched description, xrtp will write the payload into an auto generated file with filename `pt<pt>_<ssrc>.es`.

- result

    TODO

- sei

    when payload type is h.264/h.265, xrtp can write a special sei with rtp timestamp into the payload. It can help you to find the frame in the pcap file.
