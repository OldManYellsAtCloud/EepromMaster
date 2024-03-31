#include <linux/fs.h>
#include <linux/init.h>
#include <linux/kobject.h>
#include <linux/module.h>

#include <linux/device/driver.h>
#include <linux/platform_device.h>
#include <linux/of.h>

#include <linux/gpio/consumer.h>
#include <linux/of_gpio.h>

#include <linux/delay.h>

#include "eeprom.h"
#include <linux/random.h>

static struct eeprom_gpiod_device eeprom_gpio_dev;

static void eeprom_set_address(unsigned long address){
    if (address > 0x2000){
        pr_err("Address 0x%lx is invalid, max value is 0x2000.\n", address);
        return;
    }
    gpiod_set_array_value(13, eeprom_gpio_dev.address->desc, eeprom_gpio_dev.address->info, &address);
}

static void eeprom_set_value(unsigned long value){
    if (value > 0xff){
        pr_err("Value 0x%lx is invalid, max value is 0xff.\n", value);
        return;
    }
    gpiod_set_array_value(8, eeprom_gpio_dev.io->desc, eeprom_gpio_dev.io->info, &value);
}

static void eeprom_chip_enable(void){
    gpiod_set_value(eeprom_gpio_dev.ce, 0);
}

static void eeprom_chip_disable(void){
    gpiod_set_value(eeprom_gpio_dev.ce, 1);
}

static void eeprom_output_enable(void){
    gpiod_set_value(eeprom_gpio_dev.oe, 0);
}

static void eeprom_output_disable(void){
    gpiod_set_value(eeprom_gpio_dev.oe, 1);
}

static void eeprom_write_enable(void){
    gpiod_set_value(eeprom_gpio_dev.we, 0);
}

static void eeprom_write_disable(void){
    gpiod_set_value(eeprom_gpio_dev.we, 1);
}

static unsigned long eeprom_read_value(void){
    unsigned long val;
    gpiod_get_array_value(8, eeprom_gpio_dev.io->desc, eeprom_gpio_dev.io->info, &val);
    return val;
}

static uint8_t eeprom_read_byte(unsigned long address){
    int i;
    for (i = 0; i < 8; ++i)
        gpiod_direction_input(eeprom_gpio_dev.io->desc[i]);
    uint8_t ret;
    eeprom_write_disable();
    eeprom_set_address(address);
    eeprom_output_enable();
    ret = eeprom_read_value();
    return ret;
}

/*
 * After starting a write operation, IO7 shows the complement
 * of the written bit until the write operation completes.
 * This function keeps polling that bit until it flips to
 * the correct value, the indicates the end of operation, ready
 * to accept a new one.
 *
 */
static void eeprom_poll_wait_for_write(unsigned long address, uint8_t written_val){
    uint8_t bit7_of_written_val = (written_val >> 7) && 0x1;
    uint8_t tmp;
    uint8_t timeout;

    for (timeout = 0; timeout < 100; --timeout){
        tmp = eeprom_read_byte(address);
        tmp >>= 7;
        tmp &= 0x1;
        if (tmp == bit7_of_written_val)
            break;
        else
            mdelay(2);
    }

    if (timeout == 100)
        pr_err("Waiting for end of write timed out!");
}

static void eeprom_write_byte(unsigned long address, uint8_t val){
    int i;
    uint8_t bit7 = (val >> 7) & 0x1;
    uint8_t tmp;
    for (i = 0; i < 8; ++i)
        gpiod_direction_output(eeprom_gpio_dev.io->desc[i], 0);

    eeprom_output_disable();
    eeprom_set_address(address);
    eeprom_set_value(val);
    eeprom_write_enable();
    udelay(1);
    eeprom_write_disable();
    eeprom_poll_wait_for_write(address, val);
}

static void eeprom_write_bytes(unsigned long address, uint8_t* val, size_t num){
    size_t i;
    if (address + num > 0x2000){
        pr_err("Requested data does not fit eeprom!\n");
        return;
    }

    for (i = 0; i < num; ++i){
        eeprom_write_byte(address + i, val[i]);
    }
}

static int eeprom_read_bytes(unsigned long address, uint8_t* val, size_t num){
    size_t i;
    if (address + num > 0x2000){
        pr_err("Requested data can't be read!\n");
        return -1;
    }

    for (i = 0; i < num; ++i){
        val[i] = eeprom_read_byte(address + i);
    }
    return 0;
}

static void eeprom_setup(struct platform_device* pdev){
    int i;
    struct device *dev = &pdev->dev;
    eeprom_gpio_dev.ce = gpiod_get_index(dev, "ctrl", 0, GPIOD_OUT_HIGH);
    if (IS_ERR(eeprom_gpio_dev.ce)){
        pr_err("Could not get CE GPIO\n");
    }

    eeprom_gpio_dev.oe = gpiod_get_index(dev, "ctrl", 1, GPIOD_OUT_HIGH);
    if (IS_ERR(eeprom_gpio_dev.oe)){
        pr_err("Could not get OE GPIO\n");
    }

    eeprom_gpio_dev.we = gpiod_get_index(dev, "ctrl", 2, GPIOD_OUT_HIGH);
    if (IS_ERR(eeprom_gpio_dev.we)){
        pr_err("Could not get WE GPIO\n");
    }

    eeprom_gpio_dev.address = gpiod_get_array(dev, "address", GPIOD_OUT_HIGH);
    if (IS_ERR(eeprom_gpio_dev.address)){
        pr_err("Could not get Address GPIOs\n");
    }

    eeprom_gpio_dev.io = gpiod_get_array(dev, "io", GPIOD_OUT_HIGH);
    if (IS_ERR(eeprom_gpio_dev.io)){
        pr_err("Could not get IO GPIOs\n");
    }

    eeprom_chip_enable();
}


static int eeprom_probe(struct platform_device* pdev){
    eeprom_setup(pdev);
    return 0;
}

static void eeprom_remove(struct platform_device* pdev){
    gpiod_put(eeprom_gpio_dev.ce);
    gpiod_put(eeprom_gpio_dev.oe);
    gpiod_put(eeprom_gpio_dev.we);
    gpiod_put_array(eeprom_gpio_dev.address);
    gpiod_put_array(eeprom_gpio_dev.io);
}

static const struct of_device_id eeprom_match[] = {
    { .compatible = EEPROM_COMPATIBLE_STRING },
    { }
};

MODULE_DEVICE_TABLE(of, eeprom_match);

static struct platform_driver eeprom_driver = {
    .probe = eeprom_probe,
    .remove_new = eeprom_remove,
    .driver = {
        .name = "lcd1602",
        .of_match_table = eeprom_match
    }
};

static int __init eeprom_init(void){

    return platform_driver_register(&eeprom_driver);
}

static void __exit eeprom_cleanup(void){
    platform_driver_unregister(&eeprom_driver);
}

module_init(eeprom_init);
module_exit(eeprom_cleanup);

MODULE_AUTHOR("Gyorgy Sarvari");
MODULE_LICENSE("GPL");
