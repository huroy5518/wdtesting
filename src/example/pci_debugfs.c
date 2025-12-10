#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/pci.h>
#include <linux/debugfs.h>
#include <linux/slab.h>

#define DRIVER_NAME "my_pci_debugfs"

/* * Device specific structure 
 * Holds the hardware context and debugfs entries
 */
struct my_pci_dev {
    struct pci_dev *pdev;
    struct dentry *debugfs_dir;
    
    /* A dummy variable to simulate a hardware register */
    u32 config_register; 
};

/* * PCI ID Table
 * For this example, we use PCI_ANY_ID. 
 * In a real scenario, use VENDOR_ID and DEVICE_ID.
 */
static const struct pci_device_id my_pci_ids[] = {
    { PCI_DEVICE(PCI_ANY_ID, PCI_ANY_ID) },
    { 0, }
};
MODULE_DEVICE_TABLE(pci, my_pci_ids);

/*
 * Probe function: Called when the OS finds a matching device
 */
static int my_pci_probe(struct pci_dev *pdev, const struct pci_device_id *id)
{
    struct my_pci_dev *my_dev;
    int ret;

    pr_info("%s: Probing device %s\n", DRIVER_NAME, pci_name(pdev));

    /* 1. Enable PCI device */
    ret = pci_enable_device(pdev);
    if (ret) {
        pr_err("%s: Failed to enable PCI device\n", DRIVER_NAME);
        return ret;
    }

    /* 2. Allocate memory for our device state */
    my_dev = kzalloc(sizeof(struct my_pci_dev), GFP_KERNEL);
    if (!my_dev) {
        pci_disable_device(pdev);
        return -ENOMEM;
    }

    my_dev->pdev = pdev;
    my_dev->config_register = 0xCAFEBABE; // Initial dummy value

    /* 3. Setup DebugFS */
    // Create a directory: /sys/kernel/debug/my_pci_debugfs_0000:00:01.0
    // We append the PCI ID to the name to handle multiple devices gracefully
    char name_buf[64];
    snprintf(name_buf, sizeof(name_buf), "%s_%s", DRIVER_NAME, pci_name(pdev));
    
    my_dev->debugfs_dir = debugfs_create_dir(name_buf, NULL);
    
    if (IS_ERR(my_dev->debugfs_dir)) {
        pr_warn("%s: Failed to create debugfs directory\n", DRIVER_NAME);
        // We usually don't fail the probe just because debugfs failed
        my_dev->debugfs_dir = NULL;
    } else {
        /* * Create a file 'config_reg' linked to my_dev->config_register.
         * 0644 = Read/Write permissions.
         * The helper debugfs_create_u32 automatically handles read/write logic.
         */
        debugfs_create_u32("config_reg", 0644, my_dev->debugfs_dir, &my_dev->config_register);
    }

    /* 4. Save our private data into the pci_dev structure */
    pci_set_drvdata(pdev, my_dev);

    pr_info("%s: Device probed successfully\n", DRIVER_NAME);
    return 0;
}

/*
 * Remove function: Called when the module is unloaded or device removed
 */
static void my_pci_remove(struct pci_dev *pdev)
{
    struct my_pci_dev *my_dev = pci_get_drvdata(pdev);

    if (my_dev) {
        /* Clean up DebugFS - recursive removes files inside the dir too */
        debugfs_remove_recursive(my_dev->debugfs_dir);
        
        kfree(my_dev);
    }

    pci_disable_device(pdev);
    pr_info("%s: Device removed\n", DRIVER_NAME);
}

/* PCI Driver Registration Structure */
static struct pci_driver my_pci_driver = {
    .name = DRIVER_NAME,
    .id_table = my_pci_ids,
    .probe = my_pci_probe,
    .remove = my_pci_remove,
};

/* Module Initialization */
static int __init my_pci_init(void)
{
    pr_info("%s: Module loaded\n", DRIVER_NAME);
    return pci_register_driver(&my_pci_driver);
}

/* Module Exit */
static void __exit my_pci_exit(void)
{
    pci_unregister_driver(&my_pci_driver);
    pr_info("%s: Module unloaded\n", DRIVER_NAME);
}

module_init(my_pci_init);
module_exit(my_pci_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Gemini Example");
MODULE_DESCRIPTION("Simple PCI Driver with DebugFS");