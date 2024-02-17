#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/buffer_head.h>

#include "ouichefs.h"
#include "eviction_policy/eviction_policy.h"

struct size_data {
	struct inode *parent;
	struct inode *child;
};

/**
 * leaf_action - Perform an action on a leaf node during traversal
 *
 * @parent: The parent node of the leaf node.
 * @child: The leaf node to perform the action on.
 * @data: The data associated with the traversal.
 *
 * This function is called during traversal of a file system tree on a leaf node.
 * It compares the size of the child inode with the size of the current biggest file and updates the
 * biggest file if necessary.
 */
static void leaf_action(struct traverse_node *parent,
			struct traverse_node *child, void *data)
{
	struct size_data *to_del = (struct size_data *)data;

	pr_info("Leaf: %s\tsize: %lld\n", child->file->filename,
		child->inode->i_size);

	// If this inode is in use, skip it
	if (ouichefs_file_in_use(child->inode)) {
		pr_info("Skipping inode: %lu, it's in use\n",
			child->inode->i_ino);
		return;
	}

	if (to_del->child == NULL) {
		to_del->parent = parent->inode;
		to_del->child = child->inode;

		pr_info("New biggest file is: %s in directory: %s\n",
			child->file->filename, parent->file->filename);
		return;
	}

	if (to_del->child->i_size < child->inode->i_size) {
		to_del->parent = parent->inode;
		to_del->child = child->inode;

		pr_info("New biggest file is: %s in directory: %s\n",
			child->file->filename, parent->file->filename);
	}

	// TODO: possible optimization: if the file is 4 KiB (max possible) break the search
}

/**
 * clean_partition - Cleans the partition by removing a file from a directory.
 *
 * @sb: The super_block structure pointer.
 *
 * This function is responsible for cleaning the partition by removing a file
 * from a directory. It reads the directory index block on disk, traverses the
 * directory structure to find the biggest file and removes it.
 *
 * Return: 0 on success, -EIO on failure.
 */
static int clean_partition(struct super_block *sb)
{
	struct buffer_head *bh = NULL;
	struct ouichefs_dir_block *dir_block = NULL;

	struct ouichefs_inode *root_wich_inode = get_root_inode(sb);

	if (!root_wich_inode) {
		pr_info("No root inode\n");
		return -EIO;
	}

	if (!root_wich_inode->index_block)
		return -EIO;

	/* Read the directory index block on disk */
	bh = sb_bread(sb, root_wich_inode->index_block);
	if (!bh)
		return -EIO;
	dir_block = (struct ouichefs_dir_block *)bh->b_data;

	// Prepare for search in file tree

	struct ouichefs_file root_file = {
		.filename = "/",
		.inode = 0,
	};

	struct inode *root_inode = ouichefs_iget(sb, 0);

	struct traverse_node root_node = {
		.file = &root_file,
		.inode = root_inode,
	};

	struct size_data to_del = { .child = NULL, .parent = root_inode };

	// Search for the biggest file in the file tree

	traverse_dir(sb, dir_block, &root_node, NULL, NULL, leaf_action,
		     &to_del);

	// Check if anything has been found

	if (to_del.child == NULL) {
		pr_info("No file to delete\n");
		goto cleanup;
	}

	if (to_del.child == root_inode) {
		pr_info("Can't delete root directory\n");
		goto cleanup;
	}

	pr_info("Removing file: %lu in directory: %lu\n", to_del.child->i_ino,
		to_del.parent->i_ino);

	if (ouichefs_remove_file(to_del.parent, to_del.child)) {
		pr_err("Failed to remove file\n");
	}

cleanup:
	iput(root_inode);
	brelse(bh);

	return 0;
}

/**
 * clean_dir - Clean a directory by removing the biggest file
 *
 * @sb: The super block of the file system.
 * @parent: The parent inode of the directory.
 * @files: Array of ouichefs_file structures representing the files in the directory.
 *
 * This function cleans a directory by removing the file with the biggest size.
 * It iterates through the files in the directory, finds the file with the biggest size,
 * and removes it from the file system.
 * If there are no files in the directory, an error message is printed and -1 is returned.
 *
 * Return: 0 on success, -1 if there are no files in the directory
 */
static int clean_dir(struct super_block *sb, struct inode *parent,
		     struct ouichefs_file *files)
{
	struct inode *child = NULL;
	struct ouichefs_file *child_f = NULL;

	struct inode *inode = NULL;
	struct ouichefs_file *f = NULL;

	for (int i = 0; i < OUICHEFS_MAX_SUBFILES; i++) {
		f = &(files[i]);

		if (!f->inode)
			break;

		inode = ouichefs_iget(sb, f->inode);

		if (S_ISDIR(inode->i_mode))
			goto cont;

		if (!child) {
			child = inode;
			child_f = f;
			goto cont;
		}

		if (child->i_size < inode->i_size) {
			child = inode;
			child_f = f;
		}

cont:
		iput(inode);
		continue;
	}

	if (!child) {
		pr_err("No files in directory. Can't free space\n");
		return -1;
	}

	pr_info("Removing file: %s in directory: %s\n", child_f->filename,
		parent->i_sb->s_id);

	if (ouichefs_remove_file(parent, child)) {
		pr_err("Failed to remove file\n");
		return -1;
	}

	return 0;
}

static struct ouichefs_eviction_policy wich_size_policy = {
	.name = "wich_size",
	.clean_dir = clean_dir,
	.clean_partition = clean_partition,
	.list_head = LIST_HEAD_INIT(wich_size_policy.list_head),
};

static int __init my_module_init(void)
{
	pr_info("Registering size based eviction policy!\n");

	if (register_eviction_policy(&wich_size_policy)) {
		pr_err("register_eviction_policy failed\n");
		return -1;
	}

	return 0;
}
module_init(my_module_init);

static void __exit my_module_exit(void)
{
	unregister_eviction_policy(&wich_size_policy);

	printk(KERN_INFO "Unregistered size based eviction policy\n");
}
module_exit(my_module_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("mastermakrela & rico_stanosek");
MODULE_DESCRIPTION("Size based eviction policy for ouichefs");
