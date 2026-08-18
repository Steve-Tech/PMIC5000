// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Driver for Jedec PMIC5000 compliant temperature sensors
 *
 * Copyright (c) 2026 Stephen Horvath
 *
 * Inspired by spd5118.c.
 *
 * PMIC5000 compliant temperature sensors are typically used on DDR5
 * memory modules.
 */

#include <linux/bitops.h>
#include <linux/bits.h>
#include <linux/err.h>
#include <linux/i2c.h>
#include <linux/hwmon.h>
#include <linux/module.h>
#include <linux/pm.h>
#include <linux/regmap.h>

/* PMIC5000 registers. */
#define PMIC5000_REG_SWA_POWER		0x0C
#define PMIC5000_REG_SWB_POWER		0x0D
#define PMIC5000_REG_SWC_POWER		0x0E
#define PMIC5000_REG_SWD_POWER		0x0F
#define PMIC5000_REG_OUTPUT_SELECT	0x1A
#define PMIC5000_REG_CURR_OR_PWR	0x1B
#define PMIC5000_REG_SWA_CURR_WARN	0x1C
#define PMIC5000_REG_SWB_CURR_WARN	0x1D
#define PMIC5000_REG_SWC_CURR_WARN	0x1E
#define PMIC5000_REG_SWD_CURR_WARN	0x1F
#define PMIC5000_REG_ADC_CONFIG		0x30
#define PMIC5000_REG_ADC_VOLTAGE	0x31
#define PMIC5000_REG_REVISION		0x3B
#define PMIC5000_REG_VENDOR		0x3C

#define PMIC5000_OUTPUT_SELECT		BIT(1)
#define PMIC5000_CURR_OR_PWR		BIT(6)
#define PMIC5000_ADC_ENABLE		BIT(7)
#define PMIC5000_ADC_SELECT_MASK	GENMASK(6, 3)

/* 125 mW multiplier */
#define PMIC5000_POWER_UNIT		125000
/* 125 mA multiplier */
#define PMIC5000_CURR_UNIT		125
/* mV multipliers */
#define PMIC5000_VOLT_UNIT		15
#define PMIC5000_VINBULK_UNIT		70
#define PMIC5000_VBIAS_UNIT		25

struct pmic5000_data {
	struct regmap *regmap;
};

/* hwmon */

static int pmic5000_read_enable(struct regmap *regmap, long *val)
{
	u32 regval;
	int err;

	err = regmap_read(regmap, PMIC5000_REG_ADC_CONFIG, &regval);
	if (err < 0)
		return err;
	*val = !!(regval & PMIC5000_ADC_ENABLE);
	return 0;
}

static int pmic5000_read_curr(struct regmap *regmap, u32 attr, int channel, long *val)
{
	int reg, err;
	int shift = 0;
	u32 regval;
	
	if (attr == hwmon_curr_input) {
		switch (channel) {
		case 0:
			reg = PMIC5000_REG_SWA_POWER;
			break;
		case 1:
			reg = PMIC5000_REG_SWB_POWER;
			break;
		case 2:
			reg = PMIC5000_REG_SWC_POWER;
			break;
		case 3:
			reg = PMIC5000_REG_SWD_POWER;
			break;
		default:
			return -EOPNOTSUPP;
		}
	} else if (attr == hwmon_curr_crit) {
		shift = 2;
		switch (channel) {
		case 0:
			reg = PMIC5000_REG_SWA_CURR_WARN;
			break;
		case 1:
			reg = PMIC5000_REG_SWB_CURR_WARN;
			break;
		case 2:
			reg = PMIC5000_REG_SWC_CURR_WARN;
			break;
		case 3:
			reg = PMIC5000_REG_SWD_CURR_WARN;
			break;
		default:
			return -EOPNOTSUPP;
		}
	} else {
		return -EOPNOTSUPP;
	}

	err = regmap_read(regmap, reg, &regval);
	if (err)
		return err;

	*val = regval * PMIC5000_CURR_UNIT >> shift;
	return 0;
}

static int pmic5000_read_power(struct regmap *regmap, u32 attr, int channel, long *val)
{
	int reg, err;
	u32 regval;

	if (attr != hwmon_power_input)
		return -EOPNOTSUPP;

	switch (channel) {
	case 0:
		reg = PMIC5000_REG_SWA_POWER;
		break;
	case 1:
		reg = PMIC5000_REG_SWB_POWER;
		break;
	case 2:
		reg = PMIC5000_REG_SWC_POWER;
		break;
	case 3:
		reg = PMIC5000_REG_SWD_POWER;
		break;
	default:
		return -EOPNOTSUPP;
	}

	err = regmap_read(regmap, reg, &regval);
	if (err)
		return err;

	*val = regval * PMIC5000_POWER_UNIT;
	return 0;
}

static int pmic5000_read_adc(struct regmap *regmap, u32 attr, int channel, long *val)
{
	int err, mult;
	u32 regval;

	if (attr != hwmon_in_input)
		return -EOPNOTSUPP;
		
	if (channel < 0 || channel > 9 || channel == 4)
		return -EOPNOTSUPP;

	switch (channel) {
	case 5:
		mult = PMIC5000_VINBULK_UNIT;
		break;
	case 7:
		mult = PMIC5000_VBIAS_UNIT;
		break;
	default:
		mult = PMIC5000_VOLT_UNIT;
		break;
	}

	err = regmap_update_bits(regmap, PMIC5000_REG_ADC_CONFIG, PMIC5000_ADC_SELECT_MASK, channel << 3);
	if (err)
		return err;

	/* The host shall wait minimum of 9 ms delay after the input selection for ADC readout and the actual readout */
	udelay(9000);

	err = regmap_read(regmap, PMIC5000_REG_ADC_VOLTAGE, &regval);
	if (err)
		return err;

	*val = regval * mult;
	return 0;
}

static int pmic5000_read_interval(struct regmap *regmap, u32 attr, long *val)
{
	unsigned int regval;
	int err;

	if (attr != hwmon_chip_update_interval)
		return -EOPNOTSUPP;

	err = regmap_read(regmap, PMIC5000_REG_ADC_CONFIG, &regval);
	if (err < 0)
		return err;
	*val = 1 << (regval & 0x03);
	return 0;
}

static int pmic5000_read(struct device *dev, enum hwmon_sensor_types type,
			u32 attr, int channel, long *val)
{
	struct regmap *regmap = dev_get_drvdata(dev);

	switch (type) {
		case hwmon_chip:
			return pmic5000_read_interval(regmap, attr, val);
		case hwmon_in:
			return pmic5000_read_adc(regmap, attr, channel, val);
		case hwmon_curr:
			return pmic5000_read_curr(regmap, attr, channel, val);
		case hwmon_power:
			return pmic5000_read_power(regmap, attr, channel, val);
		default:
			return -EOPNOTSUPP;
	}
}

static int pmic5000_write_enable(struct regmap *regmap, long val)
{
	if (val && val != 1)
		return -EINVAL;

	return regmap_update_bits(regmap, PMIC5000_REG_ADC_CONFIG,
				  PMIC5000_ADC_ENABLE,
				  val ? PMIC5000_ADC_ENABLE : 0);
}

static int pmic5000_write(struct device *dev, enum hwmon_sensor_types type,
			 u32 attr, int channel, long val)
{
	struct regmap *regmap = dev_get_drvdata(dev);

	if (type == hwmon_power && attr == hwmon_power_enable)
		return pmic5000_write_enable(regmap, val);
	else
		return -EOPNOTSUPP;
}

static umode_t pmic5000_is_visible(const void *data, enum hwmon_sensor_types type,
				  u32 attr, int channel)
{
	struct regmap *regmap = (struct regmap *)data;
	u32 regval;
	switch (type) {
	case hwmon_chip:
		if (attr == hwmon_chip_update_interval)
			return 0644;
		break;
	case hwmon_in:
		if (attr == hwmon_in_input) {
			if (channel != 4)
				return 0444;
			else
				return 0;
		}
		break;
	case hwmon_power:
		if (attr == hwmon_power_input) {
			regmap_read(regmap, PMIC5000_REG_CURR_OR_PWR, &regval);
			if ((regval & PMIC5000_CURR_OR_PWR) == PMIC5000_CURR_OR_PWR)
				return 0444;
			else
				return 0;
		}
		break;
	case hwmon_curr:
		if (attr == hwmon_curr_input) {
			regmap_read(regmap, PMIC5000_REG_CURR_OR_PWR, &regval);
			if ((regval & PMIC5000_CURR_OR_PWR) != PMIC5000_CURR_OR_PWR)
				return 0444;
			else
				return 0;
		}
		break;
	default:
		break;
	}
	return 0444;
}

/*
 * Bank and vendor id are 8-bit fields with seven data bits and odd parity.
 * Vendor IDs 0 and 0x7f are invalid.
 * See Jedec standard JEP106BJ for details and a list of assigned vendor IDs.
 */
static bool pmic5000_vendor_valid(u8 bank, u8 id)
{
	if (parity8(bank) == 0 || parity8(id) == 0)
		return false;

	id &= 0x7f;
	return id && id != 0x7f;
}

static const struct hwmon_channel_info *pmic5000_info[] = {
	HWMON_CHANNEL_INFO(chip, HWMON_C_UPDATE_INTERVAL),
	HWMON_CHANNEL_INFO(in,
			   HWMON_I_INPUT, HWMON_I_INPUT, HWMON_I_INPUT, HWMON_I_INPUT,
			   HWMON_I_INPUT, HWMON_I_INPUT, HWMON_I_INPUT, HWMON_I_INPUT,
			   HWMON_I_INPUT, HWMON_I_INPUT),
	HWMON_CHANNEL_INFO(curr,
			   HWMON_C_INPUT | HWMON_C_CRIT,
			   HWMON_C_INPUT | HWMON_C_CRIT,
			   HWMON_C_INPUT | HWMON_C_CRIT,
			   HWMON_C_INPUT | HWMON_C_CRIT),
	HWMON_CHANNEL_INFO(power,
			   HWMON_P_INPUT, HWMON_P_INPUT,
			   HWMON_P_INPUT, HWMON_P_INPUT),
	NULL
};

static const struct hwmon_ops pmic5000_hwmon_ops = {
	.is_visible = pmic5000_is_visible,
	.read = pmic5000_read,
	.write = pmic5000_write,
};

static const struct hwmon_chip_info pmic5000_chip_info = {
	.ops = &pmic5000_hwmon_ops,
	.info = pmic5000_info,
};

/* regmap */

static bool pmic5000_writeable_reg(struct device *dev, unsigned int reg)
{
	switch (reg) {
	case PMIC5000_REG_ADC_CONFIG:
		return true;
	default:
		return false;
	}
}

static bool pmic5000_volatile_reg(struct device *dev, unsigned int reg)
{
	switch (reg) {
	case PMIC5000_REG_SWA_POWER:
	case PMIC5000_REG_SWB_POWER:
	case PMIC5000_REG_SWC_POWER:
	case PMIC5000_REG_SWD_POWER:
	case PMIC5000_REG_ADC_VOLTAGE:
		return true;
	default:
		return false;
	}
}

static const struct regmap_config pmic5000_regmap8_config = {
	.reg_bits = 8,
	.val_bits = 8,
	.max_register = 0x3f,
	.writeable_reg = pmic5000_writeable_reg,
	.volatile_reg = pmic5000_volatile_reg,
	.cache_type = REGCACHE_MAPLE,
};

static int pmic5000_suspend(struct device *dev)
{
	struct pmic5000_data *data = dev_get_drvdata(dev);
	struct regmap *regmap = data->regmap;
	u32 regval;
	int err;

	/*
	 * Make sure the configuration register in the regmap cache is current
	 * before bypassing it.
	 */
	err = regmap_read(regmap, PMIC5000_REG_ADC_CONFIG, &regval);
	if (err < 0)
		return err;

	regcache_cache_bypass(regmap, true);
	regmap_update_bits(regmap, PMIC5000_REG_ADC_CONFIG, PMIC5000_ADC_ENABLE,
			   PMIC5000_ADC_ENABLE);
	regcache_cache_bypass(regmap, false);

	regcache_cache_only(regmap, true);
	regcache_mark_dirty(regmap);

	return 0;
}

static int pmic5000_resume(struct device *dev)
{
	struct pmic5000_data *data = dev_get_drvdata(dev);
	struct regmap *regmap = data->regmap;

	regcache_cache_only(regmap, false);
	return regcache_sync(regmap);
}

static DEFINE_SIMPLE_DEV_PM_OPS(pmic5000_pm_ops, pmic5000_suspend, pmic5000_resume);

static int pmic5000_common_probe(struct device *dev, struct regmap *regmap)
{
	unsigned int revision, vendor, bank;
	struct pmic5000_data *data;
	struct device *hwmon_dev;
	int err;

	data = devm_kzalloc(dev, sizeof(*data), GFP_KERNEL);
	if (!data)
		return -ENOMEM;

	err = regmap_read(regmap, PMIC5000_REG_REVISION, &revision);
	if (err)
		return err;

	err = regmap_read(regmap, PMIC5000_REG_VENDOR, &bank);
	if (err)
		return err;
	err = regmap_read(regmap, PMIC5000_REG_VENDOR + 1, &vendor);
	if (err)
		return err;
	if (!pmic5000_vendor_valid(bank, vendor))
		return -ENODEV;

	data->regmap = regmap;
	dev_set_drvdata(dev, data);

	hwmon_dev = devm_hwmon_device_register_with_info(dev, "pmic5000",
							 regmap, &pmic5000_chip_info,
							 NULL);
	if (IS_ERR(hwmon_dev))
		return PTR_ERR(hwmon_dev);

	dev_info(dev, "DDR5 PMIC sensor: vendor 0x%02x:0x%02x revision %d.%d\n",
		 bank & 0x7f, vendor, ((revision >> 4) & 0x03) + 1, ((revision >> 1) & 0x07) + 1);
	
	/* Enable individual measurements and enable ADC */
	regmap_update_bits(regmap, PMIC5000_REG_OUTPUT_SELECT, PMIC5000_OUTPUT_SELECT, PMIC5000_OUTPUT_SELECT);
	regmap_update_bits(regmap, PMIC5000_REG_ADC_CONFIG, PMIC5000_ADC_ENABLE, PMIC5000_ADC_ENABLE);
	return 0;
}

/* I2C */

static int pmic5000_i2c_init(struct i2c_client *client)
{
	struct i2c_adapter *adapter = client->adapter;

	dev_warn(&client->dev, "Initializing PMIC5000 I2C device at 0x%02x\n", client->addr);

	/*
	 * Register accesses are 8-bit, so require byte-data transactions only.
	 * Requiring WORD_DATA here rejects otherwise valid adapters.
	 */
	if (!i2c_check_functionality(adapter, I2C_FUNC_SMBUS_BYTE_DATA))
		return -ENODEV;

	return 0;
}

static int pmic5000_i2c_probe(struct i2c_client *client)
{
	struct device *dev = &client->dev;
	struct regmap *regmap;
	int err;

	err = pmic5000_i2c_init(client);
	if (err)
		return dev_err_probe(dev, err, "I2C capability check failed\n");

	regmap = devm_regmap_init_i2c(client, &pmic5000_regmap8_config);
	if (IS_ERR(regmap))
		return dev_err_probe(dev, PTR_ERR(regmap), "regmap init failed\n");

	return pmic5000_common_probe(dev, regmap);
}

static const struct i2c_device_id pmic5000_i2c_id[] = {
	{ .name = "pmic5000" },
	{ }
};
MODULE_DEVICE_TABLE(i2c, pmic5000_i2c_id);

static const struct of_device_id pmic5000_of_ids[] = {
	{ .compatible = "jedec,pmic5000", },
	{ }
};
MODULE_DEVICE_TABLE(of, pmic5000_of_ids);

static struct i2c_driver pmic5000_i2c_driver = {
	.class		= I2C_CLASS_HWMON,
	.driver = {
		.name	= "pmic5000",
		.of_match_table = pmic5000_of_ids,
		.pm = pm_sleep_ptr(&pmic5000_pm_ops),
	},
	.probe		= pmic5000_i2c_probe,
	.id_table	= pmic5000_i2c_id,
};

module_i2c_driver(pmic5000_i2c_driver);

MODULE_AUTHOR("Stephen Horvath <stephen@horvath.au>");
MODULE_DESCRIPTION("PMIC5000 driver");
MODULE_LICENSE("GPL");
