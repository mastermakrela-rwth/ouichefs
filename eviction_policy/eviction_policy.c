// SPDX-License-Identifier: GPL-2.0

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/buffer_head.h>
#include <linux/fdtable.h>

#include "../ouichefs.h"
#include "eviction_policy.h"

// MARK: - Module parameters

int trigger_threshold = 20;
module_param(trigger_threshold, int, 0644);
MODULE_PARM_DESC(trigger_threshold, "Trigger threshold for eviction policy");

// MARK: - Default eviction policy

static int clean_partition_placeholder(struct super_block *sb)
{
	pr_info("got superblock: %s\n", sb->s_id);

	return 0;
}

static int clean_dir_placeholder(struct super_block *sb, struct inode *parent,
				 struct ouichefs_file *files)
{
	pr_info("got superblock: %s\n", sb->s_id);

	return 0;
}

struct ouichefs_eviction_policy default_policy = {
	.name = "default",
	.clean_partition = clean_partition_placeholder,
	.clean_dir = clean_dir_placeholder,
	.list_head = LIST_HEAD_INIT(default_policy.list_head),
};

struct ouichefs_eviction_policy *current_policy = &default_policy;

// MARK: - Eviction policy functions

/**
 * register_eviction_policy - Register an eviction policy
 *
 * @policy: Pointer to the eviction policy structure.
 *
 * This function registers an eviction policy by adding it to the list of
 * available policies. The policy structure must be provided as an argument.
 * The function checks for a NULL pointer and the length of the policy name
 * before registering it. If the policy name is too long, an error is returned.
 * After registering the policy, it is set as the current policy.
 *
 * Return: 0 on success, -EINVAL if the policy is NULL or the policy name is too long.
 */
int register_eviction_policy(struct ouichefs_eviction_policy *policy)
{
	/* Bail out on NULL pointer */
	if (!policy)
		return -EINVAL;

	/* Check for valid policy name length */
	if (strlen(policy->name) > POLICY_NAME_LEN) {
		pr_err("policy name too long\n");
		return -EINVAL;
	}

	// we could check if the policy is already registered / if policy with this name already exists

	list_add_tail(&policy->list_head, &default_policy.list_head);

	// change to the new policy after inserting (helpful mostly for development)
	current_policy = policy;

	pr_info("registered eviction policy '%s'\n", policy->name);

	return 0;
}
EXPORT_SYMBOL(register_eviction_policy);

/**
 * unregister_eviction_policy - Unregisters an eviction policy
 *
 * @policy: Pointer to the eviction policy to unregister.
 *
 * This function unregisters an eviction policy from the system. If the @policy
 * parameter is NULL, the function returns immediately. If the @policy parameter
 * points to the default eviction policy, the function prints an error message
 * and returns without unregistering. If the @policy parameter is the currently
 * active policy, the function falls back to the default policy. Finally, the
 * function removes the policy from the list of registered policies and prints
 * an information message indicating the successful unregistration.
 */
void unregister_eviction_policy(struct ouichefs_eviction_policy *policy)
{
	/* Bail out on NULL pointer */
	if (!policy)
		return;

	/* We do not want to lose the default (and list head) */
	if (policy == &default_policy) {
		pr_err("cannot unregister default eviction policy\n");
		return;
	}

	/* If current policy is unregistered then fallback to default */
	if (current_policy == policy)
		current_policy = &default_policy;

	list_del(&policy->list_head);

	pr_info("unregistered eviction policy '%s'\n", policy->name);
}
EXPORT_SYMBOL(unregister_eviction_policy);

/**
 * set_eviction_policy - Sets the eviction policy to the specified name.
 *
 * @name: The name of the eviction policy to set.
 *
 * This function sets the eviction policy to the specified name. It searches for
 * the policy with the given name in the list of default policies. If a matching
 * policy is found, it sets the current policy to that policy and returns 0. If
 * no matching policy is found, it returns -EINVAL.
 *
 * Return: 0 on success, -EINVAL if the name is NULL or if no matching policy is found.
 */
int set_eviction_policy(const char *name)
{
	struct ouichefs_eviction_policy *policy;

	/* Bail out on NULL pointer */
	if (!name)
		return -EINVAL;

	/* Find policy by name */
	list_for_each_entry(policy, &default_policy.list_head, list_head) {
		if (strcmp(policy->name, name) == 0) {
			current_policy = policy;
			pr_info("set eviction policy to '%s'\n", name);
			return 0;
		}
	}

	pr_err("eviction policy '%s' not found\n", name);

	return -EINVAL;
}

// MARK: - Helper functions

/**
 * traverse_dir - Recursively traverses a directory and performs actions on each directory node and
 * file leaf.
 *
 * @sb: The super block of the file system.
 * @dir: The ouichefs block to traverse.
 * @dir_node: The traverse node representing the current directory.
 * @node_action_before: The function to be called before traversing a subdirectory node.
 * @node_action_after: The function to be called after traversing a subdirectory node.
 * @leaf_action: The function to be called for each leaf.
 * @data: Additional data to be passed to the action functions.
 *
 * This function recursively traverses a directory and performs actions on each node and leaf.
 * It starts from the given directory block and traverses all subdirectories and files within.
 * The provided action functions are called at specific points during the traversal.
 * The node_action_before function is called before traversing a directory node.
 * The node_action_after function is called after traversing a directory node.
 * The leaf_action function is called for each leaf node (file).
 */
void traverse_dir(struct super_block *sb, struct ouichefs_dir_block *dir,
		  struct traverse_node *dir_node,
		  void (*node_action_before)(struct traverse_node *parent,
					     void *data),
		  void (*node_action_after)(struct traverse_node *parent,
					    void *data),
		  void (*leaf_action)(struct traverse_node *parent,
				      struct traverse_node *child, void *data),
		  void *data)
{
	struct ouichefs_file *dir_file = dir_node->file;
	struct inode *dir_inode = dir_node->inode;
	struct ouichefs_file *f = NULL;
	struct inode *inode = NULL;
	struct ouichefs_dir_block *subdir = NULL;

	for (int i = 0; i < OUICHEFS_MAX_SUBFILES; i++) {
		f = &dir->files[i];

		if (!f->inode)
			break;

		// for some reason no variation of ilookup / find_inode_* works, so after 3h of debugging we give up and just grab the inode from the disk, instead of cache
		// inode = ilookup(sb, f->inode);
		inode = ouichefs_iget(sb, f->inode);

		if (S_ISDIR(inode->i_mode)) {
			struct buffer_head *bh = sb_bread(
				sb, OUICHEFS_INODE(inode)->index_block);
			if (!bh)
				return;

			subdir = (struct ouichefs_dir_block *)bh->b_data;
			struct traverse_node subdir_node = {
				.file = f,
				.inode = inode,
			};

			if (node_action_before)
				node_action_before(&subdir_node, data);

			traverse_dir(sb, subdir, &subdir_node,
				     node_action_before, node_action_after,
				     leaf_action, data);

			if (node_action_after)
				node_action_after(&subdir_node, data);

			brelse(bh);
		} else {
			if (leaf_action) {
				struct traverse_node parent = {
					.file = dir_file,
					.inode = dir_inode,
				};
				struct traverse_node child = {
					.file = f,
					.inode = inode,
				};
				leaf_action(&parent, &child, data);
			}
		}

		iput(inode);
	}
}
EXPORT_SYMBOL(traverse_dir);

/**
 * ouichefs_remove_file - Remove a file from the ouichefs filesystem.
 *
 * @parent: The parent inode of the file
 * @child: The child inode representing the file to be removed
 *
 * This function is responsible for removing a file from the ouichefs filesystem.
 * It first tries to find the dentry associated with the child inode. If found, it
 * unlinks the file using the parent inode and the dentry. If the dentry is not found,
 * it falls back to removing the file using the parent and child inodes directly.
 *
 * Return: 0 on success, negative error code on failure
 */
int ouichefs_remove_file(struct inode *parent, struct inode *child)
{
	int ret = 0;
	struct dentry *dentry = d_find_alias(child);

	if (dentry) {
		pr_info("dentry location: %p\n", dentry);

		ret = ouichefs_unlink(parent, dentry);

		pr_info("Removed file - removing dentry\n");
		d_drop(dentry);

		dput(dentry);

	} else {
		pr_err("dentry not found - removing using inode\n");

		ret = ouichefs_remove(parent, child);
	}

	return ret;
}
EXPORT_SYMBOL(ouichefs_remove_file);

/**
 * ouichefs_file_in_use - Checks if the file is in use.
 *
 * @inode: Pointer to the inode structure of the file.
 *
 * This function checks if the file is in use by any process. It checks the read
 * and write counts of the inode and returns 1 if either of them is non-zero.
 *
 * Return: 1 if the file is in use, 0 otherwise
 */
int ouichefs_file_in_use(struct inode *inode)
{
	// print i_readcount and i_writecount
	pr_info("i_readcount: %d, i_writecount: %d\n",
		inode->i_readcount.counter, inode->i_writecount.counter);

	if (atomic_read(&inode->i_writecount) ||
	    atomic_read(&inode->i_readcount)) {
		return 1;
	}

	return 0;
}
EXPORT_SYMBOL(ouichefs_file_in_use);

/**
 * ouichefs_file_in_use_legacy - Used to check if a file is in use by any process.
 * This method is nested four levels deep, please do not show this to anyone.
 * It's a dirty hack and we know it.
 *
 * @inode: Pointer to the inode structure of the file.
 *
 * This function iterates over each process and checks if the given file
 * inode is open by any of the processes. Yes, it's dirty.
 *
 * Return: 1 if the file is in use, 0 otherwise
 */
int ouichefs_file_in_use_legacy(struct inode *inode)
{
	struct task_struct *task;
	int inode_used = 0;

	// Iterate over each process
	for_each_process(task) {
		struct files_struct *files = task->files;
		struct fdtable *fdt;

		// Check if the task has files open
		if (files) {
			// Get the file descriptor table
			fdt = files_fdtable(files);

			// Iterate over each file descriptor
			for (int fd = 0; fd < fdt->max_fds; fd++) {
				struct file *file = fdt->fd[fd];

				// Check if the file is valid and its inode matches the given inode
				if (file && file->f_inode == inode) {
					inode_used = 1;
					break;
				}
			}
		}
		if (inode_used)
			break;
	}

	return inode_used;
}
