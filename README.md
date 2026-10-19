# RTSP Play Proxy

`rtsp-play-proxy` is a proxy that sits between [ffmpeg](https://www.ffmpeg.org/) client and RTSP server with archived video. This CLI tool intercepts and changes value of `Range` header sent inside `PLAY` request which allows to extract specific video fragment from recording. More information about RTSP requests and headers can be found in RFC 7826.

## Installation

Create `rtsp-play-proxy` executable in the `./bin/` directory by running the following command:

```bash
git clone https://github.com/shadowy-pycoder/rtsp-play-proxy.git
make
```

Also note that you need to have `ffmpeg` executable in your `$PATH`.

To install system-wide:

```bash
make
sudo make install
```

## Usage

```bash
rtsp-play-proxy -h
 .__ .___. __..__   .__ .        .__
 [__)  |  (__ [__)  [__)| _.  .  [__)._. _ \./  .
 |  \  |  .__)|     |   |(_]\_|  |   [  (_)/'\\_|
                            ._|               ._|
rtsp-play-proxy v0.0.1 (built for Linux x86_64)
GitHub: https://github.com/shadowy-pycoder/rtsp-play-proxy

Usage: rtsp-play-proxy [OPTIONS] URL
OPTIONS:
    -h
        Print this help message and exit
    -R <str>
        Time format for Range header (Example: "clock=20261013T140000Z-", "npt=123.45-125")
        Default:
    -U <str>
        Set custom User-Agent header
        Default:
    -o <str>
        Name of the output file
        Default: out.mp4
    -d <int>
        Duration of the output file in seconds
        Default: 0
    -p <int>
        Run proxy on specific port (Default: assigned by kernel)
        Default: 0
```

## Example

```bash
rtsp-play-proxy -R "clock=20261013T140000Z" -d 10 -p 8888 "rtsp://user:password@192.168.1.10:554/archive?id=1"
```

## Contributing

[[Back]](#table-of-contents)

Are you a developer?

- Fork the repository
- Create your feature branch: `git switch -c my-new-feature`
- Commit your changes: `git commit -am 'Add some feature'`
- Push to the branch: `git push origin my-new-feature`
- Submit a pull request

## License

[[Back]](#table-of-contents)

GPLv3

## Similar tools

- [openRTSP](http://www.live555.com/openRTSP/)
