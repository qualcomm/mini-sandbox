# Command line flags

## Filesystem management

We support four main functioning modes listed here available in both the CLI application and the library. They try to solve different problems -- in some cases we want a drop-in replacement and prefer usability instead of security, in other cases we rather a more restricted environment less usable in general but more secure. Only one of the followings can be active for a single sandbox invocation:

### Default (`-x`) 

This should be the default production mode. It'll automatically set up the best security and flexibility options etc. Since this mode tries to be a drop-in replacement, we attempt to mount most of the filesystem inside the sandbox so that you can still re-use dependencies installed in your system. However, we close certain attack vectors by not mounting/making not writable some locations. More in detail this will:

- shut down the network (except for localhost)
- mount several system mount points (/home, /bin, /lib, /etc, /var, ) as overlay (i.e., you can write there but the modifications do not affect the filesystem out of the sandbox)
- mount less-security sensitive filesystems as read-only. These are usually the network filers, e.g., /prj, etc.
- mount /tmp as read-write
- will leave the Current Working Directory writable as well as all its subfolders
- all parent folders will be accessible as overlayfs


### Custom (`-o`)

This will still chroot to the specified directory and will use overlayfs but the locations for the chroot'ed folder (the root inside the sandbox) and the overlayfs have to be provided. Use the '-d' flag to provide a folder where to store the fs tree and the '-k' to mount fs locations in overlay mode. This can be useful for custom scenarios where we want to mount only a part of our filesystem and we wanna dictate how to mount it. See the following example 

```bash
mini-sandbox -o /local/mnt/workspace/sandbox/ -d /local/mnt/workspace/sandbox/ -k /etc -k /lib -k /lib64 -k /sbin -k /bin -k /lib32 -k /usr -M /var -M /opt -- /bin/bash
```

### Hermetic (`-h`)

This chroot to the sandbox root but does not mount any filesystem by default. User will have to manually mount the needed filesystems with '-M'/'-w' to achieve custom minimal sandbox configuration for advanced scenarios. Also in this mode there's no overlay filesystem. This is a legacy from the original linux-sandbox project. See a short example:

`mini-sandbox -M /lib -M /lib64 -M /usr -M /bin -M /etc -M /proc -M /opt -M /var -h /local/mnt/workspace/abc -- /bin/bash`

### Read-only

If no flag is provided, we spawn a best-effort sandbox that re-mounts all mount points as read-only. Current Working Directory and /tmp are still writable. We do not use overlayfs and chroot. THis functioning mode is meant for debugging or very specific use cases. Example:

`mini-sandbox -- /bin/bash`

## Additional flags

### Bind Mounts

Regardless of the functioning mode you are using, you can use a combination of `-w` (mount fs as writable) or `-M` (mount fs as read). See the following command lines that show some examples:

```bash
# everything read-only but the home is mounted read & write
mini-sandbox -w $HOME  -- /bin/bash 


# mount /etc /lib /lib64 /sbin /bin /lib32 /usr as overlay , then mounts /var and /opt and /local/mnt/workspace/custom_dir as read-only
mini-sandbox -o /local/mnt/workspace/sandbox/ -d /local/mnt/workspace/sandbox/ -k /etc -k /lib -k /lib64 -k /sbin -k /bin -k /lib32 -k /usr -M /var -M /opt -M /local/mnt/workspace/custom_dir  -- /bin/bash
``` 


### Network

By default we create a different network namespace and so we won't have network inside it. If you need access to the host network interface you can use the option `-N`. If you want a more granular control over the network you 'll have to use the `tap` mode and switch to `mini-tapbox` or `libmini-tapbox`.

Once you run in `mini-tapbox` you can access the flag `-F` that accepts a list of strings representing IP addresses/ranges or domain names. This will set up a rootless firewall based on a tap device:

```bash
echo "www.google.com" > /tmp/allowed_ips
mini-tapbox -x -F /tmp/allowed_ips -- /bin/bash
wget www.google.com      # success !
wget www.wikipedia.com   # will fail !
```

