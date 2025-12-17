#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/debugfs.h>
#include <linux/fs.h>
#include <linux/uaccess.h> // For copy_from_user
#include <linux/module.h>
#include "top.h"
MODULE_LICENSE("GPL");
                           
#define WDTEST_DEBUGFS_NAME "wdtest"
static struct dentry *test_debugfs_dir;
static unsigned int config_value = 0x54455354;

static ssize_t read_result(struct file *file, char __user *user_buf,
                                size_t count, loff_t *ppos)
{
    // 1. Allocate a kernel buffer to hold your output
    //    (Make sure it's large enough for your formatted string)
    char kbuf[2048]; 
    int len;

    // 2. Format your data into the kernel buffer
    //    scnprintf is preferred over snprintf in kernel (returns actual length)
    // len = scnprintf(kbuf, sizeof(kbuf), "Counter value: %d\n", 0);
    for (int i = 0; i < MAX_TRACKED_BLOCKS; i ++) {
        if (_wd_get_begin_blk_count(i) == 0 && _wd_get_end_blk_count(i) == 0) {
            continue;
        }
        len += scnprintf(kbuf + len, sizeof(kbuf), "%d,%d,%d\n", i, _wd_get_begin_blk_count(i), _wd_get_end_blk_count(i));
    }

    // 3. Copy to user with offset handling
    //    Arg 1: Destination (User)
    //    Arg 2: Size requested by user
    //    Arg 3: Pointer to current file offset (updated automatically)
    //    Arg 4: Source (Kernel)
    //    Arg 5: Size of source data
    return simple_read_from_buffer(user_buf, count, ppos, kbuf, len);
}


static ssize_t trigger_write(struct file *file, const char __user *user_buf,
                            size_t count, loff_t *ppos)
{
    char buf[32];
    int ret;

    // Limit buffer size to prevent overflows
    int len = min(count, (size_t)sizeof(buf) - 1);
    
    RUN_TEST();

    if (copy_from_user(buf, user_buf, len))
        return -EFAULT;

    buf[len] = '\0'; // Null terminate

    // Example Logic: specific action based on input
    if (buf[0] == '1') {
        pr_info("[%s] Trigger logic activated!\n", WDTEST_DEBUGFS_NAME);
    } else {
        pr_info("[%s] Unknown command received: %s\n", WDTEST_DEBUGFS_NAME, buf);
    }

    return count;
}

static const struct file_operations gather_fops = {
    .owner = THIS_MODULE,
    .read = read_result, // We only define write here
    .open  = simple_open,  // Generic open
};

static const struct file_operations trigger_fops = {
    .owner = THIS_MODULE,
    .write = trigger_write, // We only define write here
    .open  = simple_open,  // Generic open
};

void create_test_debugfs(void) {
    pr_info("Creating wdtesting debugfs file\n");
    INIT_TEST();
    test_debugfs_dir = debugfs_create_dir(WDTEST_DEBUGFS_NAME, NULL);

    if (IS_ERR(test_debugfs_dir)) {
        pr_err("[%s] Failed to create debugfs directory\n", WDTEST_DEBUGFS_NAME);
        return;
    }
    
    debugfs_create_file("trigger_test", 0666, test_debugfs_dir, NULL, &trigger_fops);
    debugfs_create_file("gather_test_result", 0644, test_debugfs_dir, NULL, &gather_fops);
}

EXPORT_SYMBOL(create_test_debugfs);