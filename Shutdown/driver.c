/* @file  emergency_shutdown_driver.c
 * @description use to execute emergency shutdown in case of power outage
 * a gpio can be connected to trigger the emergency shutdown sequence
 */

#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/gpio.h>
#include <linux/of_device.h>
#include <linux/of_gpio.h>
#include <linux/delay.h>
#include <linux/kmod.h>
#include <linux/gpio/driver.h>
#include <linux/gpio.h>
#include <linux/interrupt.h>
#include <linux/of.h>
#include <linux/debugfs.h>
#include <linux/fs.h>

struct dentry *emg_debugfs_file_ptr;
struct delayed_work emergency_delayed_work;

static void emergency_shutdown(struct work_struct *work)
{
        int error = 0;
        char* argv[] = {"/bin/emg-shutdown", "emergency shutdown because of power outage", NULL};
        static char* envp[] = {
                "HOME=/",
                "TERM=linux",
                "PATH=/sbin:/bin:/usr/sbin:/usr/bin", NULL };

        error = call_usermodehelper(argv[0], argv, envp, UMH_WAIT_PROC);
        if(error)
                printk(KERN_ALERT"[%s]: call_usermodhelpher failed \n",__func__);

}


static irq_handler_t emg_shutdown_irq_handler(unsigned int irq){

        schedule_delayed_work(&emergency_delayed_work,0);
        return (irq_handler_t) IRQ_HANDLED;
}

static int emergency_shutdown_probe(struct platform_device *pdev)
{

        /*
         * DTS Node Example
         * shutdown {
         compatible = "emergency-shutdown";
         emgshutdown-pin = <&gpioxz pin_num 0>
         status = "okay";
         };
         */
        int result = 0;
        unsigned int emg_shutdown_pin;
        unsigned int irqNumber;
        struct device_node *np = pdev->dev.of_node;
        if(!np)
                return -ENODEV;

        INIT_DELAYED_WORK(&emergency_delayed_work, emergency_shutdown);

        emg_shutdown_pin = of_get_named_gpio(np,"emgshutdown-pin",0);
        if(!gpio_is_valid(emg_shutdown_pin)){
#ifdef CONFIG_DEBUG_FS
                printk(KERN_INFO"[%s]: gpio not configured for emergency shutdown\n",__func__);
                return 0;
#endif
                return -ENODEV;
        }

        gpio_request(emg_shutdown_pin, "emg_shutdown");
        gpio_direction_input(emg_shutdown_pin);
        gpio_set_debounce(emg_shutdown_pin, 100);
        gpio_export(emg_shutdown_pin, false);
        irqNumber = gpio_to_irq(emg_shutdown_pin);
        result = request_irq(irqNumber,
                        (irq_handler_t) emg_shutdown_irq_handler,
                        IRQF_TRIGGER_RISING,
                        "emg_shutdown_irq_handler",
                        NULL);
        printk(KERN_ALERT"[%s]: executing",__func__);
        return 0;
}

static int emergency_shutdown_remove(struct platform_device *pdev)
{
        return 0;
}

static const struct of_device_id emergency_shutdown_of_match[] ={
        {.compatible = "emergency-shutdown",},
        {/*SENTINAL*/},
};
MODULE_DEVICE_TABLE(of, emergency_shutdown_of_match);

static struct platform_driver emergency_shutdown_driver = {
        .probe          = emergency_shutdown_probe,
        .remove         = emergency_shutdown_remove,
        .driver         = {
                .name = "emergency_shutdown",
                .owner = THIS_MODULE,
                .of_match_table = emergency_shutdown_of_match,
        },
};

#ifdef CONFIG_DEBUG_FS

static ssize_t emg_debugfs_read(struct file *fp, char __user *user_buffer, size_t count, loff_t *position)
{
        printk(KERN_ALERT"[%s]: emergency debugfs read \n",__func__);
        return 0;
}

static ssize_t emg_debugfs_write(struct file *fp, const char __user *user_buffer, size_t count, loff_t *position)
{
        printk(KERN_ALERT"[%s]: emergency debugfs write \n",__func__);

        schedule_delayed_work(&emergency_delayed_work,0);

                                                        
        return 0;
}

static const struct file_operations emg_debugfs_fops = {
        .owner  = THIS_MODULE,
        .read   = emg_debugfs_read,
        .write  = emg_debugfs_write,
};

#endif

static int __init emergency_shutdown_init(void)
{
        int ret;

#ifdef CONFIG_DEBUG_FS
        emg_debugfs_file_ptr = debugfs_create_file("emergency_shutdown",0777,NULL,NULL,&emg_debugfs_fops);     // /sys/kernel/debug/emergency_shutdown
        if(!emg_debugfs_file_ptr)
                return -ENOMEM;
#endif

        ret = platform_driver_register(&emergency_shutdown_driver);
        printk(KERN_ALERT"[%s]: register ret %d \n",__func__, ret);
        return ret;

}
arch_initcall(emergency_shutdown_init);

static void __exit emergency_shutdown_exit(void)
{
#ifdef CONFIG_DEBUG_FS
        debugfs_remove(emg_debugfs_file_ptr);
#endif
        platform_driver_unregister(&emergency_shutdown_driver);
}
module_exit(emergency_shutdown_exit);


MODULE_AUTHOR("Basava Prabhu <bprabhu@qti.qualcomm.com>");
MODULE_DESCRIPTION("QTI emergency shutdown driver");
MODULE_LICENSE("GPL");
