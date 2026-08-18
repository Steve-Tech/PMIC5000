# PMIC5000 DKMS

This is a Linux kernel module for reading sensors from the PMIC5000 chip used in DDR5 memory modules. It is based on the spd5118 driver. The driver currently does not auto-detect, so you will need to manually instantiate the device.

The [it87 Makefile](https://github.com/a1wong/it87/blob/master/Makefile) was also used as an example.

## Installation

```sh
sudo make dkms
sudo modprobe pmic5000
echo pmic5000 0x49 | sudo tee /sys/bus/i2c/devices/i2c-1/new_device
```

## Uninstallation

```sh
echo 0x49 | sudo tee /sys/bus/i2c/devices/i2c-1/delete_device
sudo modprobe -r pmic5000
sudo make dkms_clean
```
