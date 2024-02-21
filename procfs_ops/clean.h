/* SPDX-License-Identifier: GPL-2.0 */

#include <linux/kernel.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>

/**
 * clean_proc_write - Write callback for cleaning a specific mount
 *
 * @s: Pointer to the file structure.
 * @buf: Pointer to the input buffer.
 * @size: Size of the input buffer.
 * @ppos: Pointer to the file position.
 *
 * This function is the write callback for cleaning a specific mount. It takes
 * the mount index from the input buffer and searches for the corresponding
 * partition. If found, it checks if the partition has a superblock and if the
 * superblock's magic number matches the expected value (ouichefs magic number).
 * If all conditions are met, it calls the clean_partition() function of the
 * current policy to clean the partition.
 *
 * Return: The size of the input buffer on success, or a negative error code on failure.
 */
static ssize_t clean_proc_write(struct file *s, const char __user *buf,
				size_t size, loff_t *ppos)
{
	int index;

	if (kstrtoint_from_user(buf, size, 10, &index) != 0) {
		pr_err("Invalid index\n");
		return -EINVAL;
	}

	if (index < 0) {
		pr_err("Invalid index - must be a positive integer\n");
		return -EINVAL;
	}

	struct partition *partition;
	struct list_head *head = &first_partition.list;
	int i = 0;

	list_for_each_entry(partition, head, list) {
		if (i == index)
			break;
		i++;
	}

	if (i != index) {
		pr_err("No partition found at index %d - out of range\n",
		       index);
		return -EINVAL;
	}

	if (partition == NULL) {
		pr_err("No partition found at index %d\n", index);
		return -EINVAL;
	}

	if (partition == NULL) {
		pr_err("No such partition found\n");
		return -EINVAL;
	}

	struct super_block *sb = partition->sb;

	if (sb == NULL) {
		pr_err("Partition without superblock - this should not happen ¯\\_(ツ)_/¯\n");
		return -EINVAL;
	}

	if (sb->s_magic != OUICHEFS_MAGIC) {
		pr_err("Partition is not ouichefs - cannot clean\n");
		return -EINVAL;
	}

	current_policy->clean_partition(sb);

	return size;
}

const struct proc_ops clean_proc_ops = {
	.proc_write = clean_proc_write,
};
