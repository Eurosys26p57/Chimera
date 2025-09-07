#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/hashtable.h>
#include <linux/hash.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/slab.h>
#include <linux/uaccess.h>

#define TRAP_HASH_BITS 8
#define MAX_LINE_LENGTH 100

struct trap_redirect_entry {
    uint64_t trap_address;
    uint64_t redirect_address;
    struct hlist_node node;
};

static DEFINE_HASHTABLE(trap_hash_table, TRAP_HASH_BITS);
static DEFINE_MUTEX(trap_hash_mutex);

static inline unsigned int trap_hash(uint64_t pc) {
    return hash_64(pc, TRAP_HASH_BITS);
}

static void handle_trap(struct pt_regs *regs) {
    uint64_t current_pc = regs->pc;
    struct trap_redirect_entry *entry;
    unsigned int hash_index = trap_hash(current_pc);

    hash_for_each_possible(trap_hash_table, entry, node, hash_index) {
        if (entry->trap_address == current_pc) {
            regs->pc = entry->redirect_address;
            break;
        }
    }
}

static int errorhandling_proc_show(struct seq_file *m, void *v) {
    struct trap_redirect_entry *entry;
    int bkt;

    mutex_lock(&trap_hash_mutex);
    hash_for_each(trap_hash_table, bkt, entry, node) {
        seq_printf(m, "0x%llx 0x%llx\n", entry->trap_address, entry->redirect_address);
    }
    mutex_unlock(&trap_hash_mutex);
    return 0;
}

static ssize_t errorhandling_proc_write(struct file *file, const char __user *buffer, size_t count, loff_t *pos) {
    char *line, *temp;
    char buf[MAX_LINE_LENGTH];
    uint64_t trap_addr, redirect_addr;
    struct trap_redirect_entry *entry, *new_entry;
    int ret = 0;

    if (count > MAX_LINE_LENGTH) {
        return -EINVAL;
    }

    if (copy_from_user(buf, buffer, count)) {
        return -EFAULT;
    }
    buf[count] = '\0';

    line = buf;
    while ((line = strchr(line, '\n'))) {
        *line++ = '\0';
    }

    line = buf;
    while (line && *line) {
        temp = strchr(line, '\n');
        if (temp) {
            *temp = '\0';
        }

        if (sscanf(line, "0x%llx 0x%llx", &trap_addr, &redirect_addr) != 2) {
            ret = -EINVAL;
            goto out;
        }

        new_entry = kmalloc(sizeof(*new_entry), GFP_KERNEL);
        if (!new_entry) {
            ret = -ENOMEM;
            goto out;
        }
        new_entry->trap_address = trap_addr;
        new_entry->redirect_address = redirect_addr;

        mutex_lock(&trap_hash_mutex);
        hash_for_each_possible(trap_hash_table, entry, node, trap_hash(trap_addr)) {
            if (entry->trap_address == trap_addr) {
                entry->redirect_address = redirect_addr;
                kfree(new_entry);
                goto next_line;
            }
        }
        hash_add(trap_hash_table, &new_entry->node, trap_hash(trap_addr));
next_line:
        mutex_unlock(&trap_hash_mutex);
        line = temp ? temp + 1 : NULL;
    }

    return count;
out:
    return ret;
}

static int errorhandling_proc_open(struct inode *inode, struct file *file) {
    return single_open(file, errorhandling_proc_show, NULL);
}

static const struct proc_ops errorhandling_proc_ops = {
    .proc_open = errorhandling_proc_show,
    .proc_read = seq_read,
    .proc_write = errorhandling_proc_write,
    .proc_lseek = seq_lseek,
    .proc_release = single_release,
};

static int __init trap_redirect_init(void) {
    proc_create("errorhandling", 0666, NULL, &errorhandling_proc_ops);
    return 0;
}

static void __exit trap_redirect_exit(void) {
    struct trap_redirect_entry *entry;
    struct hlist_node *tmp;
    int bkt;

    remove_proc_entry("errorhandling", NULL);
    hash_for_each_safe(trap_hash_table, bkt, tmp, entry, node) {
        hash_del(&entry->node);
        kfree(entry);
    }
}

module_init(trap_redirect_init);
module_exit(trap_redirect_exit);

MODULE_LICENSE("GPL");
