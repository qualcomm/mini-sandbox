
## Ubuntu Restrictions

Starting with **Ubuntu 24.04**, Ubuntu introduces a security change that limits the creation of user namespaces by unprivileged users.
This affects our usual plug‑and‑play workflow on these systems and requires manual configuration.

For more details, see the official Ubuntu blog post:  
https://ubuntu.com/blog/whats-new-in-security-for-ubuntu-24-04-lts

To overcome this limitation, we run a light-weight version of mini-sandbox which only enables capabilities. 

To fully enable the sandbox, the user manually needs to enable user namespace and export MINI_SANDBOX_FORCE_USER_NAMESPACE variable. 


## Enabling User Namespaces

To allow user namespaces system‑wide, apply the following configuration:

```bash
echo "kernel.apparmor_restrict_unprivileged_userns = 0" >/etc/sysctl.d/99-userns.conf
sysctl --system
```

Alternatively, you can define a custom app-armor profile for mini-sandbox which allows user namespaces.

```
# This profile allows userns under the default apparmor profile.
# Place in /etc/apparmor.d/mini-sandbox

abi <abi/4.0>,
include <tunables/global>

profile mini-sandbox /local/mnt/workspace/release/bin/mini-sandbox flags=(default_allow) {
        userns,
}
```

To load the new profile execute

```
sudo apparmor_parser -r /etc/apparmor.d/ /etc/apparmor.d/mini-sandbox
sudo aa-status | grep mini # Verify the outcome
```

At this point, you can set the environment variable "MINI_SANDBOX_FORCE_USER_NAMESPACE" and make sure that mini-sanbdox leverages user namespaces.


```
echo MINI_SANDBOX_FORCE_USER_NAMESPACE=1>>/etc/environment
```
