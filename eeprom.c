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

//static int lcd1602_device_major;
//static struct class* lcd1602_cls;

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

static void eeprom_write_byte_to_address(unsigned long address, uint8_t val){
    int i;
    for (i = 0; i < 8; ++i)
        gpiod_direction_output(eeprom_gpio_dev.io->desc[i], 0);
    eeprom_chip_disable();
    eeprom_output_disable();
    eeprom_write_disable();
    eeprom_set_address(address);
    eeprom_set_value(val);
    eeprom_write_enable();
    eeprom_chip_enable();
    mdelay(12);
    eeprom_write_disable();
}

static uint8_t eeprom_read_byte_from_address(unsigned long address){
    int i;
    for (i = 0; i < 8; ++i)
        gpiod_direction_input(eeprom_gpio_dev.io->desc[i]);
    uint8_t ret;
    eeprom_chip_disable();
    eeprom_output_disable();
    eeprom_write_disable();
    eeprom_set_address(address);
    eeprom_output_enable();
    eeprom_chip_enable();
    mdelay(6);
    ret = eeprom_read_value();
    return ret;
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

    //lcd1602_verify_dt(dev);
}


static int eeprom_probe(struct platform_device* pdev){
    //void* pdata = platform_get_drvdata(pdev);
    eeprom_setup(pdev);
    //lcd1602_setup_userspace(pdev);
    uint8_t buf[1];
    get_random_bytes(&buf, 1);
    pr_info("Writing this value: %d\n", buf[0]);
    eeprom_write_byte_to_address(0x100, buf[0]);
    pr_info("Reading back (should be 224): %d\n", eeprom_read_byte_from_address(0x100));

    return 0;
}

static void eeprom_remove(struct platform_device* pdev){
    //lcd1602_reset_screen();
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
    //lcd1602_reset_screen();
    //device_destroy(lcd1602_cls, MKDEV(lcd1602_device_major, 0));
    //class_destroy(lcd1602_cls);
    platform_driver_unregister(&eeprom_driver);
}

module_init(eeprom_init);
module_exit(eeprom_cleanup);

MODULE_AUTHOR("Gyorgy Sarvari");
MODULE_LICENSE("GPL");
