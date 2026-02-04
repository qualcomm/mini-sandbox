# Project overview


A sandbox CLI and library based on the concept of namespaces which can be used to isolate the untrusted code. The main use cases are isolating standalone tools like python scripts, bash , binaries etc. Say you have to run a python tool that imports dozens 3rd-party dependencies you'll never manually review, this is for you. It comes with the same advantage of Podman style isolation (root-less namespaces), but doesn't require any Containerfile/Dockerfile and can be directly embedded in your code. 

More details about the goals at [motivations](docs/motivations.md)

## Getting started

The easier way to access the tool is to download the binaries/libraries from the [release page](https://github.com/qualcomm/mini-sandbox/releases). 

Alternatively, you can also build the code, see [build instructions](docs/build.md).

Here we are going to highlight three main uses cases

### Default Command line usage

The fastest, default usage is:

`mini-sandbox -x -- /bin/bash`

In short this will i) shut down the network 2) mount several mount points as overlay (modifications do not affect the filesystem out of the sandbox) 3) mount less-security sensitive or custom filesystems as read-only (e.g., autofs, nfs, ..) 4) chroot into a custom folder .
If you need to open network connections in your app/package check out the `TAP mode` below to enable the root-less firewall feature that will allow you to block all connections except for the necessary ones.

**IMPORTANT** If you execute this from hour $HOME directory we will mount the $HOME directory as fully writable and many of our isolation guarantees do not hold anymore.

### TAP mode -- root-less firewall

By leveraging the gVisor framework by Google, we manage to use a tap interface and a TCP/IP network stack to be able to run a firewall inside our sandbox. To have this feature you need to build the `mini-tapbox` binary (see `docs/build.md`). Once the binary has been built usage is straightforward -- just collect the list of IP addresses, domain names, subnets needed for your app into a file and use the -F flag (see example below)

```bash
echo "www.google.com" > /tmp/allowed_ips
mini-tapbox -x -F /tmp/allowed_ips -- /bin/bash
wget www.google.com  # success !
wget wikipedia.com   # will fail !
```


### Library Mode -- Python

Last, we have bindings for C/C++ and Python. In this section we'll walk through an example for out python bindings but you can checkout the C/C++ APIs in [the APIs documentation](docs/libminisandbox_apis.md).

You can install our PyPI package with `pip install pyminisandbox`.

```python
import pyminisandbox.pyminitapbox as mn_sbx

if __name__ == "__main__":
        
    mn_sbx.mini_sandbox_setup_default()

    mn_sbx.mini_sandbox_mount_write("/usr2")
    mn_sbx.mini_sandbox_allow_domain("www.wikipedia.org")
 
    res = mn_sbx.mini_sandbox_start()
    assert (res == 0)
     
    print("Running inside the sandbox...")
    attempt_network_connection("http://www.google.com") 
    attempt_network_connection("http://www.wikipedia.org")
 
    # rest of your logic
```


## Comparison with other projects

This project is heavily inspired by few other open-source projects that have been proposed over the last few years. However, we haven't found any of these projects to fulfill all our necessities and that's why we decided to work on a different implementation in the attempt to maximize the deployability and usability of the project. In the following table you can see an informal comparison of what features we considered for our evaluation and how the other projects correlate to those. To keep the comparison more consistent we are also going to highlight few features that we do not currently support but keep in mind that we are not claiming to be better than the other tools, simply that we try to look at a different features set.


| Tool   | Root-less | Drop-in replacement | overlay FS | root-less firewall | library | Seccomp | Full-system container |
| :----- | :-------: | :-----------------: | :--------: | :----------------: | :-----: | :-----: |  :------------------: |
| Docker | :x:       | :x:                 | ✅         |  :x:              | :x:      |  ✅    |   ✅ |
| Podman/root-less Docker | ✅ | :x: | ✅   | ✅  | :x:     |  ✅ | ✅ |
| nsjail | ✅ | ✅ | :x: | :x: | :x: | ✅ | :x: |
| Sandboxed API | ✅ | ✅  | :x: | :x: | ✅ | ✅ | :x: |
| Bubblewrap | ✅ | ✅ | :x: | :x: | :x: | ✅ | :x: |
| **mini-sandbox** | ✅ | ✅ |✅ | :x: | ✅ | :x: | :x: |
| **mini-tapbox** | ✅ | ✅ |✅ | ✅ | ✅ | :x: | :x: |

## License

mini-sandbox is licensed under the [MIT License](https://spdx.org/licenses/MIT.html). See [License.txt](LICENSE.txt) for the full license text.
