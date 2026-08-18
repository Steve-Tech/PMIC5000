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
$ sensors -u pmic5000-*
pmic5000-i2c-1-4b
Adapter: SMBus PIIX4 adapter port 0 at 0b00
SWA:
  in0_input: 1.350
  in0_min_alarm: 0.000
  in0_max_alarm: 0.000
SWB:
  in1_input: 0.000
  in1_min_alarm: 0.000
  in1_max_alarm: 0.000
SWC:
  in2_input: 1.365
  in2_min_alarm: 0.000
  in2_max_alarm: 0.000
SWD:
  in3_input: 1.815
  in3_min_alarm: 0.000
  in3_max_alarm: 0.000
VIN_Bulk:
  in5_input: 4.970
  in5_min_alarm: 0.000
VIN_Mgmt:
  in6_input: 0.000
  in6_min_alarm: 0.000
VBias:
  in7_input: 0.000
VOUT_1.8V:
  in8_input: 1.800
VOUT_1.0V:
  in9_input: 1.005
temp1:
  temp1_input: 95.000
  temp1_max_alarm: 0.000
SWA:
  power1_input: 0.625
SWB:
  power2_input: 0.000
SWC:
  power3_input: 0.125
SWD:
  power4_input: 0.000
SWA:
  curr1_input: 0.500
  curr1_max: 3.000
  curr1_max_alarm: 0.000
SWB:
  curr2_input: 0.000
  curr2_max: 0.000
  curr2_max_alarm: 0.000
SWC:
  curr3_input: 0.125
  curr3_max: 3.000
  curr3_max_alarm: 0.000
SWD:
  curr4_input: 0.000
  curr4_max: 3.000
  curr4_max_alarm: 0.000
```
