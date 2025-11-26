# Command line flags

## Filesystem management

We support 5 main functioning modes here listed by security (first is the most secure and does not require much manual configuration, the last is less secure). Only one of the followings can be active for a single sandbox invocation:

- '-x' this should be the default production mode. It'll automatically set up the best security and flexibility options such as chroot, overlayfs, no network connection (or firewall), etc. Most of the cases you want to just use this flag

- '-o' the "manual" mode of the previous one -- this will still chroot to the specified directory and will use overlayfs but the locations have to be provided. Use the '-d' flag to provide a folder where to store the fs tree and the '-k' to mount fs locations in overlay mode. This is in most of the cases useless cause '-x' already does these actions automatically, but can be use for custom scenarios where we may want to mount mostly filesystem as read-write and only few in overlay mode (example of this command line `/mini-sandbox -o /local/mnt/workspace/sandbox_root/ciao -d /local/mnt/workspace/sandbox_tmpfs/t/a  -k /afs -k /boot -k /etc -k /lib -k /lib64 -k /sbin -k /srv -k /bin -k /lib32 -k /libx32 -k /usr -M /proc -M /var -M /opt -- /bin/bash`)

- '-h sandbox_root' this chroot to sandbox_root but does not mount any filesystem by default. User will have to manually mount the needed filesystems with '-M'/'-w' to achieve custom minimal sandbox configuration for advanced scenarios (example for a minimal working environment: `mini-sandbox -M /lib -M /lib64 -M /usr -M /bin -M /etc -M /proc -M /opt -M /var -h /local/mnt/workspace/abc -- /bin/bash`). OverlayFS is not available in this scenario.

- No flags - if no flag is provided, we spawn a best-effort sandbox that re-mounts all mount points as read-only. We do not use overlayfs and chroot to pivot the home directory. THis functioning mode is meant for debugging or very specific use cases but NOT FOR PRODUCTION.
