
# Motivations

Modern cybersecurity attacks infiltrate into the supply chain of a certain product to perform a lateral movement and hack into other orgs at scale. The most popular example is the SolarWinds attack, 2021, where attackers injected malicious code into the SolarWinds products which then were shipped to approx 18K clients. Once the trojanised products reached the clients' machine the malicious code was activating , downloading additional malicious content, exfiltrating data and communicating with the attackers' servers.

The main takeaway of this attack is that the malicious actors here did not target the end consumers (i.e., the final user of a smartphone or a laptop), but what they cared the most was the access into all those 18K organizations -- in other words the intermediate business between SolarWinds and the consumers. This is also why such an attack had a tremendous financial impact among incident response activities, investigations, lawsuits, insurances, government agencies hacked, etc.

Thinking about a generic corporate scenario we have essentially three major categories of code that gets releases to clients :

1. Code running in the builds which are made on the OEMs' side (Android, Software Products, ..). This is definitely a good place where an attacker might inject malware to infect someone else's machines and network cause it's code running inside the OEMs network. We classify this as first priority
2. Standalone tools for monitoring/diagnostics/debugging. This is also code that will end up in the OEMs' machines and therefore has to be treated as first-class citizen.
3. Code for the Software Product that will exclusively run on the final device therefore not really useful to hack into the OEMs. We assume this code will run in a different machine than the code 1. and 2. and be subject to different security policies, etc. Thus, this is out of scope of mini-sandbox


# Solution

Code running at the OEMs side tends to explode in dimensions, especially due to the large amount of third-party dependencies that our proprietary codebases still leverage to perform a certain tasks. Scanning each script/tool shipped outside is not scaling and therefore we decided to apply a containment strategy called sandboxing.

The basic idea is that we reduce the privileges of code running inside our sandbox. For instance a python script running inside our container won't be able to arbitrarily modify files that are present in the home directories. There are few other restrictions that apply here mostly at the filesystem level, network level and user level. Most of the restrictions are configurable even though we have a default mode where we try to enforce all the important restrictions to make sure the sandbox guarantees are at the top. Here the list of restrictions that we enforce inside our sandbox when running it in default mode:
1. Overlay filesystems for /bin, /lib, /etc and few other fundamentals
2. CWD writable as a regular user
3. Parents of CWD read-only 
4. Home never accessible, unless specified differently
5. Rest of the filesystem mounted as read-only except for autofs that will be left up to the system administrator
6. Isolate pid, mount, proc namespaces
7. Isolate network with two functioning modes: default (no network at all) or pasta mode (user-space firewall to select the needed IPs)

Conceptually, as explained in the introduction, this effort resembles a docker container, but we have simplified the usage so that people can just place it in their scripts without the need to change their environments.


