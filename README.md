# PMIC5000 DKMS

This is a Linux kernel module for reading sensors from the PMIC5000 chip used in DDR5 memory modules. The driver currently does not auto-detect, so you will need to manually instantiate the device (on my system the PMIC is at address `0x49` & `0x4b` on PIIX4 port 0).

The [it87 Makefile](https://github.com/a1wong/it87/blob/master/Makefile) was also used as an example.

## Installation

```sh
sudo make dkms
sudo modprobe pmic5000
echo pmic5000 0x49 | sudo tee /sys/bus/i2c/devices/i2c-1/new_device
echo pmic5000 0x4b | sudo tee /sys/bus/i2c/devices/i2c-1/new_device
```

## Uninstallation

```sh
echo 0x49 | sudo tee /sys/bus/i2c/devices/i2c-1/delete_device
echo 0x4b | sudo tee /sys/bus/i2c/devices/i2c-1/delete_device
sudo modprobe -r pmic5000
sudo make dkms_clean
```

## Example Output

```
$ sensors pmic5000-*
pmic5000-i2c-1-4b
Adapter: SMBus PIIX4 adapter port 0 at 0b00
SWA:           1.36 V  (min =  +1.22 V, max =  +1.52 V)
SWC:           1.36 V  (min =  +1.22 V, max =  +1.52 V)
SWD:           1.81 V  (min =  +1.62 V, max =  +2.02 V)
VIN_Bulk:      4.97 V  (max = +16.00 V)
VIN_Mgmt:      0.00 V  (max =  +3.70 V)
VBias:         0.00 V
VOUT_1.8V:     1.80 V
VOUT_1.0V:     1.00 V
temp1:            N/A  (high = +125.0°C)
SWA:         625.00 mW
SWC:         125.00 mW
SWD:           0.00 W
SWA:         500.00 mA (max =  +3.00 A)
SWC:         125.00 mA (max =  +3.00 A)
SWD:           0.00 A  (max =  +3.00 A)
```

```
$ sensors -u pmic5000-*
pmic5000-i2c-1-4b
Adapter: SMBus PIIX4 adapter port 0 at 0b00
SWA:
  in0_input: 1.350
  in0_min: 1.215
  in0_max: 1.518
  in0_min_alarm: 0.000
  in0_max_alarm: 0.000
SWC:
  in2_input: 1.365
  in2_min: 1.215
  in2_max: 1.518
  in2_min_alarm: 0.000
  in2_max_alarm: 0.000
SWD:
  in3_input: 1.800
  in3_min: 1.620
  in3_max: 2.025
  in3_min_alarm: 0.000
  in3_max_alarm: 0.000
VIN_Bulk:
  in5_input: 4.970
  in5_max: 16.000
  in5_max_alarm: 0.000
VIN_Mgmt:
  in6_input: 0.000
  in6_max: 3.700
  in6_max_alarm: 0.000
VBias:
  in7_input: 0.000
VOUT_1.8V:
  in8_input: 1.800
VOUT_1.0V:
  in9_input: 0.990
temp1:
ERROR: Can't get value of subfeature temp1_input: Can't read
  temp1_max: 125.000
  temp1_max_alarm: 0.000
SWA:
  power1_input: 0.625
SWC:
  power3_input: 0.125
SWD:
  power4_input: 0.000
SWA:
  curr1_input: 0.500
  curr1_max: 3.000
  curr1_max_alarm: 0.000
SWC:
  curr3_input: 0.125
  curr3_max: 3.000
  curr3_max_alarm: 0.000
SWD:
  curr4_input: 0.000
  curr4_max: 3.000
  curr4_max_alarm: 0.000
```
