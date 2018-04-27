/* dvc2_usb_watchdog.c
 *
 * Configure USB_HUB_DET as an interrupt.  Watch for falling edge, and
 * assert USB_RESET_N (active low) for a short period of time.
 *
 * Device tree example:-
 *
 * dvc2-usb-watchdog {
 *         compatible = "dvc2-usb-watchdog";
 *         interrupt-parent = <&gpio1>;
 *         interrupts = <3 4>;
 *         reset-gpios = <&gpio3 22 0>;
 * };
 *
 */

#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/gpio.h>
#include <linux/of.h>
#include <linux/delay.h>

MODULE_AUTHOR("Robert Hawkins <robert.hawkins@spx.com>");
MODULE_DESCRIPTION("USB hub watchdog for RD DVC2");
MODULE_VERSION("0.1");
MODULE_LICENSE("GPL");

struct dvc2_usb_watchdog_data {
	struct platform_device *pdev;
	struct gpio_desc *reset_gpio;
};

static const struct of_device_id dvc2_usb_watchdog_dt_ids[] = {
	{ .compatible = "dvc2-usb-watchdog", },
	{ }
};
MODULE_DEVICE_TABLE(of, dvc2_usb_watchdog_dt_ids);

static irqreturn_t dvc2_usb_watchdog_irq_handler(int irq, void *dev_id)
{
	struct dvc2_usb_watchdog_data *data = dev_id;
	struct device *dev = &data->pdev->dev;
	
	if (!IS_ERR(data->reset_gpio)) {
		dev_err(dev, "resetting USB hub");
		msleep(200);
		gpiod_set_value(data->reset_gpio, 0);
		msleep(1);
		gpiod_set_value(data->reset_gpio, 1);
	} else {
		dev_err(dev, "unable to reset USB hub");
	}
	msleep(2000);
	return IRQ_HANDLED;
}

static int dvc2_usb_watchdog_probe(struct platform_device *pdev)
{
	struct dvc2_usb_watchdog_data *data;
	struct device *dev = &pdev->dev;
	struct device_node *np = dev->of_node;
	int ret = 0;
	int irq;

	if (!np) {
		dev_err(dev, "no dvc2-usb-watchdog device node");
		return -EINVAL;
	}

	data = devm_kzalloc(dev, sizeof(*data), GFP_KERNEL);
	if (!data)
		return -ENOMEM;

	data->pdev = pdev;

	/* Set up USB_RESET_N gpio */
	data->reset_gpio = devm_gpiod_get(dev, "reset");
	if (IS_ERR(data->reset_gpio)
	    || gpiod_direction_output(data->reset_gpio, 1) < 0)
		dev_err(dev, "failed to set up USB_HUB_RESET gpio");

	irq = platform_get_irq(pdev, 0);
	if (irq < 1) {
		dev_err(dev, "platform_get_irq() failed to get IRQ for USB_HUB_DET");
		return -EINVAL;
	}

	ret = devm_request_threaded_irq(dev, irq, NULL,
			       dvc2_usb_watchdog_irq_handler,
			       IRQF_TRIGGER_LOW | IRQF_SHARED | IRQF_ONESHOT,
			       dev_name(dev), data);
	if (ret) {
		dev_err(&pdev->dev, "unable to claim irq %d; error %d\n", irq, ret);
		return -EINVAL;
	}
	dev_info(dev, "USB hub watchdog initialised on irq %d", irq);

	return 0;
}

/*
static int dvc2_usb_watchdog_remove(struct platform_device *pdev)
{
	struct dvc2_usb_watchdog_data *data = platform_get_drvdata(pdev);
	if (!IS_ERR(data->reset_gpio))
		gpiod_put(data->reset_gpio);
}
*/

static struct platform_driver dvc2_usb_watchdog_driver = {
	.driver         = {
		.name   = "dvc2-usb-watchdog",
		.owner  = THIS_MODULE,
		.of_match_table = of_match_ptr(dvc2_usb_watchdog_dt_ids),
	},
	.probe          = dvc2_usb_watchdog_probe,
	/* .remove         = dvc2_usb_watchdog_remove, */
};

static int __init dvc2_usb_watchdog_init(void)
{
	printk (KERN_INFO "USB hub watchdog init\n");
        return platform_driver_register(&dvc2_usb_watchdog_driver);
}

static void __exit dvc2_usb_watchdog_exit(void)
{
	printk (KERN_INFO "USB hub watchdog exit\n");
	platform_driver_unregister(&dvc2_usb_watchdog_driver);
}

module_init(dvc2_usb_watchdog_init);
module_exit(dvc2_usb_watchdog_exit);

