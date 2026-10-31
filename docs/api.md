# Radio HTTP API

Base URL `http://<radio-ip>/api`

| Method | Path      | Request body | Success      |
| :----- | :-------- | :----------- | :----------- |
| GET    | `/status` | none         | `200` Status |
| GET    | `/config` | none         | `200` Config |
| POST   | `/config` | see below    | `200` Config |
| POST   | `/reboot` | none         | `200` Ok     |

## Status

`<link>` is `halow`, `wifi_2_4` or `wifi_5`, and `<radio>` is `local` or `peer`.

- read only, this is how the basestation gets telemetry
- the radio keeps no history, so it reports running counts and the basestation computes rates from the change between polls
- every count is a uint64 since boot
- `peer` is null until the first heartbeat
- should be totally achievable my firmware

| Field                             | Type          | Values                                                       |
| :-------------------------------- | :------------ | :----------------------------------------------------------- |
| `links.<link>.state`              | string        | `down`, `associating`, `up`                                  |
| `links.<link>.state_s`            | int           | seconds passed since current state                           |
| `links.halow.sub_band`            | string        | `low`, `mid`, `high`                                         |
| `links.wifi_<band>.channel`       | int           | channel number, band dependent                               |
| `links.<link>.bw_mhz`             | int           |                                                              |
| `links.<link>.rtt_ms`             | float or null | for heartbeats, null if none yet                             |
| `links.<link>.heartbeats_sent`    | int           |                                                              |
| `links.<link>.heartbeats_acked`   | int           |                                                              |
| `<radio>.role`                    | string        | `base`, `rover`, changes only on reboot                      |
| `<radio>.ip`                      | string        | IPv4                                                         |
| `<radio>.uptime_ms`               | int           |                                                              |
| `<radio>.fw`                      | string        | fw version                                                   |
| `<radio>.links.<link>.rssi_dbm`   | int or null   |                                                              |
| `<radio>.links.<link>.mcs`        | int or null   |                                                              |
| `<radio>.links.<link>.tx_bytes`   | int           |                                                              |
| `<radio>.links.<link>.rx_bytes`   | int           |                                                              |
| `<radio>.links.<link>.tx_packets` | int or null   | packets sent                                                 |
| `<radio>.links.<link>.tx_failed`  | int or null   | packets not acked after all retries                          |
| `<radio>.links.<link>.txq_depth`  | int           | packets queued                                               |
| `<radio>.links.<link>.txq_drops`  | int           | packets dropped from the queue                               |
| `<radio>.system.temp_c`           | int           |                                                              |
| `<radio>.system.heap_free`        | int           | bytes                                                        |
| `<radio>.system.heap_min`         | int           | bytes, lowest since boot to show max usage                   |
| `<radio>.system.cm7_idle_us`      | int           | idle time, for CPU utilization                               |
| `<radio>.system.cm4_idle_us`      | int           | idle time, for CPU utilization                               |
| `<radio>.system.eth`              | string        | `down`, `10H`, `10F`, `100H`, `100F`, detects bad hardware   |
| `<radio>.system.reset_reason`     | string array  | any of `power_on`, `pin`, `software`, `watchdog`, `brownout` |

## Config

| Field                   | Type        | Values                |
| :---------------------- | :---------- | :-------------------- |
| `role`                  | string      | `base`, `rover`       |
| `net.ip`                | string      | IPv4                  |
| `net.mask`              | string      | IPv4, /8 to /30       |
| `link.passphrase_set`   | bool        | read only             |
| `halow.sub_band`        | string      | `low`, `mid`, `high`  |
| `halow.bw_mhz`          | int         | 1, 2, 4, 8            |
| `halow.tx_power_dbm`    | int         |                       |
| `halow.mcs`             | int or null | 0 to 7, null for auto |
| `wifi_2_4.channel`      | int         | 1 to 11               |
| `wifi_2_4.bw_mhz`       | int         | 20, 40                |
| `wifi_2_4.tx_power_dbm` | int         |                       |
| `wifi_5.channel`        | int         | US channels           |
| `wifi_5.bw_mhz`         | int         | 20, 40, 80            |
| `wifi_5.tx_power_dbm`   | int         |                       |

- `halow.sub_band` is the competition sub-band, `low` 902 to 910, `mid` 911 to 919, `high` 920 to 928 MHz
- the channel is centered in the sub-band at 906, 915 or 924 MHz
- narrower widths trade throughput for range and leave more margin inside the sub-band edges

## ConfigPatch

Any subset* of config, as well as:

`link.passphrase`, 8 to 63 chars

*see error handling below

| Fields                                      | Applied to      | Takes effect   |
| :------------------------------------------ | :-------------- | :------------- |
| `role`, `net`                               | this radio only | on next reboot |
| `link`                                      | both radios     | on next reboot |
| `halow.sub_band`, `halow.bw_mhz`            | both radios     | immediately    |
| `wifi_<band>.channel`, `wifi_<band>.bw_mhz` | both radios     | immediately    |
| `halow.tx_power_dbm`, `halow.mcs`           | this radio only | immediately    |
| `wifi_<band>.tx_power_dbm`                  | this radio only | immediately    |

- both radios must share a frequency and width for a link to come up, so those fields are synced
- transmit power and MCS is scoped only to the local radio
- to change the peer's own fields, send the request to `peer.ip` from Status

### Error Handling

- a field sent with its current value is not a change, and none of these rules apply to it
- reject `net` values that are invalid with `400 out_of_range`
- if it's a both radio change, and the peer is unreachable, reject with `502 peer_unreachable`
- a link's shared fields change only while another link is up, otherwise `409 other_link_down`
  - a failed retune on the only working link would leave the radios unable to reach each other
- do not allow changes to the shared fields of more than one link at a time, reject with `400 both_links_changed`

## Ok

sample:

```json
{ "ok": true }
```

## Error

`field` is the path of the offending option, or null

sample:

```json
{
  "error": "out_of_range",
  "field": "halow.bw_mhz",
  "reason": "expected 1, 2, 4 or 8"
}
```

Error table

| Status | `error`                                                                       |
| :----- | :---------------------------------------------------------------------------- |
| 400    | `bad_json`, `unknown_field`, `bad_type`, `out_of_range`, `both_links_changed` |
| 409    | `other_link_down`                                                             |
| 500    | `internal`                                                                    |
| 502    | `peer_unreachable`                                                            |
