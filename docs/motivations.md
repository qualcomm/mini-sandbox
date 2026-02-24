
# Motivations

Modern cybersecurity attacks infiltrate into the supply chain of a certain product to perform a lateral movement and hack into other orgs at scale. The most popular example is the SolarWinds attack, 2021, where attackers injected malicious code into the SolarWinds products which then were shipped to approx 18K clients. Once the trojanised products landed into the clients' machine the malicious code was activating , downloading additional malicious content, exfiltrating data and communicating with the attackers' servers.

The main takeaway of this attack is that the malicious actors here did not target the end consumers (i.e., the final user of a smartphone or a laptop), but what they cared the most was the access into all those 18K organizations -- in other words the intermediate business between SolarWinds and the consumers. This is also why such an attack had a tremendous financial impact among incident response activities, investigations, lawsuits, insurances, government agencies hacked, etc.

In the modern software ecosystem we routinely get overwhelmed by 3rd-party, open or close-source software. Manually reviewing each single component executing inside our machine is unfeasible and detection-based approaches do not properly scale (false positives, dynamic analysis issues, etc.). 


# Solution

Rather than scanning each script/tool used looking for some `signs` of maliciousness or unpatched bugs, we decided to apply a containment strategy called sandboxing and here implemented in a tool named `mini-sandbox` (or its library-based version `libmini-sandbox`). The main underlying idea behind the tool is that it should be as much "transparent" as possible for the end-user, without requiring much overhead to enable it on a new tool. As said, other tools exist with similar scope but they all didn't fit our use case where ease-of-use becomes a crucial factor. Few examples:
- One may be tempted to use Docker/Podman to run tools in an isolated context but that requires to write everytime a Dockerfile, pull images and create a brand-new environment from scratch. This requires a certain overhead
- Other tools such as nsjail and similars are great sandboxes to begin with but they are mostly command line only and to the best of our knowledge require root for the firewall configuration
- Sandboxed API is another great example of novel sandboxing techniques but you have to manually modify your code in order to run your functions inside the sandbox -- again, we don't want to require changes to the code.

The basic idea is that we reduce the privileges of code running inside our sandbox as already done by many others and we introduce different restrictions, depending on the running mode. 
Our restrictions apply mostly at the filesystem, network, user and IPC levels. Most of the restrictions are configurable even though we have a default mode where we try to enforce all the important restrictions. Here the list of restrictions that we enforce inside our sandbox when running it in default mode (CWD is Current Working Directory):
1. Read-only filesystems for /bin, /lib, /etc
2. CWD writable as a regular user
3. Parents of CWD mounted as overlay filesystem
4. Home's original data never accessible, unless specified differently (for instance if you're executing from the home itself)
5. Rest of the filesystem mounted as read-only except for autofs that will be left up to the host machine configuration
6. Isolate network with two functioning modes: default (no network at all) or tap mode (user-space firewall to select the needed IPs)
7. Isolate pid, mount, proc namespaces


Conceptually, as explained in the introduction, this effort resembles a docker container, but we have simplified the usage so that `mini-sandbox` can be used as a drop-in solution instead of bothering with Dockerfile, sudo access, etc.
