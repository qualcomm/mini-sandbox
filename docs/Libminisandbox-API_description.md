# Libminisandbox APIs 

## Logging

### `int mini_sandbox_enable_log(std::string path);`

Enables logging for the sandbox system.  
**Parameters:**
- `path`: Path to the log file.

**Returns:** `0` on success, non-zero on failure.

---

## Sandbox Setup

These functions configure the sandbox environment. Choose the appropriate setup based on your use case.

### `int mini_sandbox_setup_default();`

Sets up the sandbox with default settings, including overlay filesystem and chroot.

### `int mini_sandbox_setup_custom(std::string overlayfs_dir, std::string sandbox_root);`

Allows specifying custom directories for the overlay filesystem and sandbox root.  
**Parameters:**
- `overlayfs_dir`: Directory for overlay filesystem.
- `sandbox_root`: Root directory for the sandbox.

### `int mini_sandbox_setup_hermetic(std::string sandbox_root);`

Creates a fully isolated (hermetic) sandbox rooted at the specified path.

### `int mini_sandbox_setup_readonly();`

Configures the sandbox with a read-only filesystem.

---

## Sandbox Execution

### `int mini_sandbox_start();`

Parses configuration, unshares namespaces, and forks a new sandboxed process.  
**Returns:** `0` on success, non-zero on failure.

---

## Mounting Paths

These functions control how paths are mounted inside the sandbox.

### `int mini_sandbox_mount_bind(std::string path);`

Mounts the specified path as a bind mount.

### `int mini_sandbox_mount_write(std::string path);`

Mounts the path with write permissions, allowing output from the sandbox.

### `int mini_sandbox_mount_read_only(std::string path);`

Mounts the path as read-only (default behavior).

### `int mini_sandbox_mount_overlay(std::string path);`

Mounts the path using an overlay filesystem.

### `int mini_sandbox_mount_empty_output_file(std::string path)`

The file specified in path is created if doesn't exist, and mounted in write mode inside the sandbox. 
This function is extremely useful if the script or API writes to an arbitrary file and we don't want to mount the whole directory in write mode.

CAVEATS: If the scripts deletes the file or need to create it, this will generate an error.

---

## Firewall Configuration (Only in libminitapbox)

These APIs are available only when compiled with minitap enabled (i.e., `make libmini-tapbox`)

### `void mini_sandbox_set_firewall_rules(const char* path);`

Set rules from a file.

### `void mini_sandbox_set_firewall_rule(const char* rule);`

Add an individual rule.

### `void mini_sandbox_allow_ipv4(const char* rule);`

Allow a specific IPv4 address.

### `void mini_sandbox_allow_domain(const char* rule);`

Allow a specific domain.

### `void mini_sandbox_allow_all_domains();`

Allow all domains.

### `void mini_sandbox_allow_ipv4_subnet(const char* rule);`

Allow an IPv4 subnet.

---


