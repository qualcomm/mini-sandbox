# Project overview

A sandbox CLI and library based on the concept of namespaces which can be used to isolate the untrusted code. The main use cases are isolating standalone tools like python scripts, bash , binaries etc. Say you have to run a python tool that imports dozens 3rd-party dependencies you'll never manually review, this is for you. It comes with the same advantage of Podman style isolation (root-less namespaces), but doesn't require any Containerfile/Dockerfile and can be directly embedded in your code. 

More details about the goals at [motivations](docs/motivations.md)

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




## Getting started

Here we are going to highlight three 

## Build

See build instructions at `docs/Build.md`

## Usage

The fastest, default usage is:

`mini-sandbox -x -- /bin/bash`

In short this will i) shut down the network 2) mount several filesystems as overlay (modifications do not affect the filesystem out of the sandbox) 3) mount less-security sensitive filesystems as read-only (/prj) 4) chroot into a custom folder .
If you need to open network connections in your app/package check out the `TUN mode` below to enable the user-space firewall feature that will allow you to block all connections except for the necessary ones.

### custom CLI mode -- usage

If you need more customization, instead of using the `-x` flag you can use a combination of `-w` (mount fs as writable), `-M` (mount fs as read) and `-k` (mount fs as overlay). See the following command line as an example: 

`mini-sandbox -w $(pwd) -w /tmp -w /dev/shm -o /local/mnt/workspace/sandbox -d /local/mnt/workspace/overlayfs -D /tmp/dbg -N -k /afs -k /boot -k /etc -k /lib -k /lib64 -k /mnt -k /root -k /sbin -k /srv -k /tmp -k /bin -k /cm -k /emul -k /lib32 -k /libx32 -k /pkg -k /usr -M /proc -M /var -M /opt -- /bin/bash`

`/var` and `/opt` are both needed to resolve the username

More details about the CLI at `docs/Flags.md`


### TUN mode -- user-space firewall

By leveraging the gVisor framework by Google, we manage to use a tap interface and a TCP/IP network stack to be able to run a firewall inside our sandbox. To have this feature you need to run the following commands from the root of this folder. Once the binary has been built usage is straightforward -- just collect the list of IP addresses, domain names, subnets needed for your app into a file and use the -F flag (see example below)

```
make mini-tapbox
```

Example usage:

```
echo "142.250.189.14" > /tmp/allowed_ips
./out/mini-tapbox -x -F /tmp/allowed_ips -- /bin/bash
ping 142.250.189.14  // google.com - ok
ping wikipedia.com   // will fail !
```

### Library mode

Library APIs are availabe for importing in C, C++ and Python codebases. Have a look at `docs/Libminisandbox.md` and `docs/Libminisandbox-API_description.md`
