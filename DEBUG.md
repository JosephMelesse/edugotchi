# falcons_tomodachi — Debug Guide

## HTTP request not going through

### Step 1 — Check the URL scheme
Always use `http://`, never `https://`. The ESP's web server does not support SSL.

- `http://172.20.10.8/startAlarm` ✓
- `https://172.20.10.8/startAlarm` ✗ (times out silently)

### Step 2 — Check the server is running
```
curl -s --max-time 5 http://172.20.10.6:3000/question?difficulty=easy
```
If it times out, restart the server:
```
cd /home/joseph-melesse/ideahacks/server
node index.js
```

### Step 3 — Check the machine's current IP
```
hostname -I
```
The first IP (e.g. `172.20.10.6`) must match `QUESTION_URL` in `falcons_tomodachi.ino`. If it changed, update the `#define` and reflash.

### Step 4 — Check the ESP connected to WiFi
Open the serial monitor:
```
arduino-cli monitor -p /dev/ttyACM0 --config baudrate=115200
```
Look for `IP: 172.20.10.x`. If you see `Wi-Fi unavailable`, the SSID/password in the `.ino` doesn't match the hotspot.

### Step 5 — Check both devices are on the same network
The laptop and ESP must be on the same hotspot. Ping the ESP from the laptop:
```
ping -c 3 172.20.10.8
```
If it times out, one of them is on a different network.

### Step 6 — Check the firewall
```
sudo ufw status
sudo ufw allow 3000/tcp
```

---

## ESP URLs
| Action | URL |
|---|---|
| Trigger alarm | `http://<ESP-IP>/startAlarm` |
| Stop alarm | `http://<ESP-IP>/stopAlarm` |
| Status | `http://<ESP-IP>/status` |
| Set difficulty | `http://<ESP-IP>/setDifficulty?level=easy\|medium\|hard` |

ESP IP is printed on serial at boot (`IP: ...`).

---

## Reflash command
```
arduino-cli compile --upload -b esp32:esp32:esp32s3:CDCOnBoot=cdc -p /dev/ttyACM0 /home/joseph-melesse/ideahacks/falcons_tomodachi
```
