```bash
fw/scripts/build.sh -s fw/mradio -p Release
cmake -DHALOW_DEFAULT_ROLE=ap fw/mradio/CM4/build
fw/scripts/build.sh -s fw/mradio -p Release -f
```

```bash
fw/scripts/build.sh -s fw/mradio -p Release
cmake -DHALOW_DEFAULT_ROLE=sta fw/mradio/CM4/build
fw/scripts/build.sh -s fw/mradio -p Release -f
```

```bash
picocom -b 115200 /dev/ttyACM0
```
