# Radio HTTP API

Base URL `http://<radio-ip>/api`

| Method | Path      | Request body | Success      |
| :----- | :-------- | :----------- | :----------- |
| GET    | `/status` | none         | `200` Status |
| GET    | `/config` | none         | `200` Config |
| POST   | `/config` | see below    | `200` Config |
| POST   | `/reboot` | none         | `200` Ok     |

## Status

`<link>` is `halow` or `wifi`, and `<radio>` is `local` or `peer`.

- read only, this is how the basestation gets telemetry
- the radio keeps no history, so it reports running counts and the basestation computes rates from the change between polls
- every count is a uint64 since boot
- `peer` is null until the first heartbeat
- should be totally achievable my firmware

| Field                             | Type          | Values                                                       |
| :-------------------------------- | :------------ | :----------------------------------------------------------- |
| `links.<link>.state`              | string        | `down`, `associating`, `up`                                  |
| `links.<link>.state_s`            | int           | seconds passed since current state                           |
| `links.halow.center_khz`          | int           |                                                              |
| `links.wifi.band`                 | string        | `2.4`, `5`                                                   |
| `links.wifi.channel`              | int           | channel number, band dependent                               |
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

| Field                 | Type        | Values                                   |
| :-------------------- | :---------- | :--------------------------------------- |
| `role`                | string      | `base`, `rover`                          |
| `net.ip`              | string      | IPv4                                     |
| `net.mask`            | string      | IPv4 contiguous                          |
| `link.passphrase_set` | bool        | read only                                |
| `halow.center_khz`    | int         | multiple of 500                          |
| `halow.bw_mhz`        | int         | 1, 2, 4, or 8                            |
| `halow.tx_power_dbm`  | int         |                                          |
| `halow.mcs`           | int or null | 0 to 7, null for auto                    |
| `wifi.band`           | string      | `2.4`, `5`                               |
| `wifi.channel`        | int         | 1 to 11 at 2.4 GHz, US channels at 5 GHz |
| `wifi.bw_mhz`         | int         | 20, 40, 80. 80 at 5 GHz only             |
| `wifi.tx_power_dbm`   | int         |                                          |

## ConfigPatch

Any subset* of config, as well as:

`link.passphrase`, 8 to 63 chars

*see error handling below

| Fields                                                 | Applied to      | Takes effect   |
| :----------------------------------------------------- | :-------------- | :------------- |
| `role`, `net`                                          | this radio only | on next reboot |
| `link`                                                 | both radios     | on next reboot |
| `halow.center_khz`, `halow.bw_mhz`                     | both radios     | immediately    |
| `wifi.band`, `wifi.channel`, `wifi.bw_mhz`             | both radios     | immediately    |
| `halow.tx_power_dbm`, `halow.mcs`, `wifi.tx_power_dbm` | this radio only | immediately    |

- both radios must share a frequency and width for a link to come up, so those fields are synced
- transmit power and MCS is scoped only to the local radio
- to change the peer's own fields, send the request to `peer.ip` from Status

### Error Handling

- a field sent with its current value is not a change, and none of these rules apply to it
- reject `net` values that are invalid with `400 out_of_range`
- if it's a both radio change, and the peer is unreachable, reject with `502 peer_unreachable`
- shared `halow` fields change only while `wifi` is up and vice versa, otherwise `409 other_link_down`
  - a failed retune on the only working link would leave the radios unable to reach each other
- do not allow changes to shared `halow` and `wifi` fields at the same time, reject with `400 both_links_changed`

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
  "field": "halow.center_khz",
  "reason": "channel 905000 to 913000 crosses a sub-band boundary"
}
```

Error table

| Status | `error`                                                                       |
| :----- | :---------------------------------------------------------------------------- |
| 400    | `bad_json`, `unknown_field`, `bad_type`, `out_of_range`, `both_links_changed` |
| 409    | `other_link_down`                                                             |
| 500    | `internal`                                                                    |
| 502    | `peer_unreachable`                                                            |
