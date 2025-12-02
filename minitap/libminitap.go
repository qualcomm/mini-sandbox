

package main

import (
        "C"
        "unsafe"
	"context"
	"errors"
	"fmt"
	"log"
	"net"
	"os"
	"os/exec"
	"runtime"
	"syscall"
        "golang.org/x/sys/unix"
	"strings"
        "bufio"
        "sync"

	"github.com/vishvananda/netlink"
	"gvisor.dev/gvisor/pkg/tcpip"
	"gvisor.dev/gvisor/pkg/tcpip/adapters/gonet"
	"gvisor.dev/gvisor/pkg/tcpip/header"
	"gvisor.dev/gvisor/pkg/tcpip/link/fdbased"
	"gvisor.dev/gvisor/pkg/tcpip/network/ipv4"
	"gvisor.dev/gvisor/pkg/tcpip/network/ipv6"
	"gvisor.dev/gvisor/pkg/tcpip/stack"
        "gvisor.dev/gvisor/pkg/tcpip/link/tun"
	"gvisor.dev/gvisor/pkg/tcpip/transport/icmp"
	"gvisor.dev/gvisor/pkg/tcpip/transport/tcp"
	"gvisor.dev/gvisor/pkg/tcpip/transport/udp"
	"gvisor.dev/gvisor/pkg/waiter"
)

const (
	dumpPacketsToSubprocess   = false
	dumpPacketsFromSubprocess = false
	ttl                       = 10
)

var isVerbose bool = false;

type FirewallTool int


type FirewallRules struct {
        Rules []string
        Count int
}

const (
        NONE FirewallTool = iota
        IPTABLES_NFT
        NFT
)

const (
        NFT_RULESET        = "/tmp/nft-ruleset.conf"
        MAX_ARGS           = 64
        SUCCESS            = 0
        ERR_EXEC_FAILED    = -1
        ERR_FILE_NOT_FOUND = -2
)

var Tool FirewallTool = NONE

var (
    fwRules FirewallRules
    mu      sync.Mutex
)


func fileExists(path string) bool {
    info, err := os.Stat(path)
    return err == nil && !info.IsDir()
}


func MiniTapSetupFirewallRule(rule string) {
    mu.Lock()
    defer mu.Unlock()
    fwRules.Rules = append(fwRules.Rules, rule)
    fwRules.Count++
}


func ReadFirewallRules(firewall_rules string) {
    file, err := os.Open(firewall_rules)
    if err != nil {
        fmt.Printf("Failed to open file: %s\n", err)
        return
    }
    defer file.Close()

    scanner := bufio.NewScanner(file)
    for scanner.Scan() {
        rule := scanner.Text()
        MiniTapSetupFirewallRule(rule)
    }

    if err := scanner.Err(); err != nil {
        fmt.Printf("Error reading file: %s\n", err)
    }
}


func whichNFTFirewall() FirewallTool {
        if Tool != NONE {
                return Tool
        }

        if envTool := os.Getenv("FIREWALL_TOOL"); envTool != "" {
                switch envTool {
                case "IPTABLES_NFT":
                        Tool = IPTABLES_NFT
                case "NFT":
                        Tool = NFT
                default:
                        Tool = NONE
                }
                return Tool
        }

        if _, err := os.Stat("/usr/sbin/iptables-nft"); err == nil {
                Tool = IPTABLES_NFT
        } else if _, err := os.Stat("/usr/sbin/nft"); err == nil {
                Tool = NFT
        } else {
                Tool = NONE
        }
        return Tool
}

func executeFirewallRule(rule string) int {
        args := append([]string{"iptables-nft"}, strings.Fields(rule)...)
        cmd := exec.Command(args[0], args[1:]...)
        if err := cmd.Run(); err != nil {
                fmt.Fprintf(os.Stderr, "Execution failed: %v\n", err)
                return ERR_EXEC_FAILED
        }
        return SUCCESS
}

func setupNFTFile() (*os.File, int) {
        fp, err := os.Create(NFT_RULESET)
        if err != nil {
                fmt.Fprintf(os.Stderr, "Failed to open file: %v\n", err)
                return nil, ERR_FILE_NOT_FOUND
        }
        fp.WriteString("table ip pasta {\n")
        fp.WriteString("    chain output {\n")
        fp.WriteString("        type filter hook output priority 0;\n")
        fp.WriteString("        policy drop;\n")
        return fp, SUCCESS
}

func writeFirewallRule(fp *os.File, rule string) int {
        if fp != nil {
                fp.WriteString(fmt.Sprintf("        %s;\n", rule))
                return SUCCESS
        }
        return ERR_FILE_NOT_FOUND
}

func closeNFTFile(fp *os.File) {
        fp.WriteString("    }\n")
        fp.WriteString("}\n")
        fp.Close()
}

func executeNFT(path string) {
        if path == "" {
                fmt.Fprintln(os.Stderr, "Error: ruleset path is NULL.")
                os.Exit(1)
        }
        cmd := exec.Command("/usr/sbin/nft", "-f", path)
        if err := cmd.Run(); err != nil {
                fmt.Fprintf(os.Stderr, "execv failed: %v\n", err)
                os.Exit(1)
        }
}


func ExecuteFirewallRules() {
        if fwRules.Count == 0 {
                return
        }

        tool := whichNFTFirewall()
        if tool == NONE {
                fmt.Println("Warning: no nft based firewall (iptables-nft or nft). Couldn’t set up firewall in namespace")
                return
        }

        var fp *os.File
        if tool == NFT {
                var res int
                fp, res = setupNFTFile()
                if res < 0 {
                        return
                }
        }

        for i := fwRules.Count - 1; i >= 0; i-- {
                rule := fwRules.Rules[i]
                fmt.Printf("Executing rule: %s, Rule index %d\n", rule, i)
                var result int
                switch tool {
                case IPTABLES_NFT:
                        result = executeFirewallRule(rule)
                case NFT:
                        result = writeFirewallRule(fp, rule)
                default:
                        result = ERR_FILE_NOT_FOUND
                }
                if result != SUCCESS {
                        fmt.Fprintf(os.Stderr, "Failed to setup rule: %s\n", rule)
                }
        }

        if tool == NFT {
                closeNFTFile(fp)
                executeNFT(NFT_RULESET)
        }
}

func verbose(msg string) {
	if isVerbose {
		log.Print(msg)
	}
}

func verbosef(fmt string, parts ...interface{}) {
	if isVerbose {
		log.Printf(fmt, parts...)
	}
}


func errorf(s string, parts ...interface{}) {
	if !strings.HasSuffix(s, "\n") {
		s += "\n"
	}
        fmt.Printf("Error:")
        fmt.Printf(s)
        
}


func setEnv(envVars []string) {
        
   for _, env := range envVars {
        parts := strings.SplitN(env, "=", 2)
        if len(parts) != 2 {
            fmt.Printf("Invalid env format: %s\n", env)
            continue
        }

        key := parts[0]
        value := parts[1]

        err := os.Setenv(key, value)
        if err != nil {
            fmt.Printf("Failed to set %s: %v\n", key, err)
        }
    }
}

type cfg struct {
	Tun                string 
	Subnet             string 
	Gateway            string 
	UID                int
	GID                int
	HTTPPorts          []int  
	HTTPSPorts         []int  
}


var config = cfg{
    Tun:        "",
    Subnet:     "",
    Gateway:    "",
    UID:        -1,
    GID:        -1,
    HTTPPorts:  nil,
    HTTPSPorts: nil,
}

//export MiniTapSetTun
func MiniTapSetTun(tun *C.char) {
    config.Tun = C.GoString(tun)
}

//export MiniTapSetSubnet
func MiniTapSetSubnet(subnet *C.char) {
    config.Subnet = C.GoString(subnet)
}

//export MiniTapSetGateway
func MiniTapSetGateway(gateway *C.char) {
    config.Gateway = C.GoString(gateway)
}

//export MiniTapSetUID
func MiniTapSetUID(uid C.int) {
    config.UID = int(uid)
}

//export MiniTapSetGID
func MiniTapSetGID(gid C.int) {
    config.GID = int(gid)
}

//export MiniTapSetHTTP
func MiniTapSetHTTP(ports *C.int, length C.int) {
    slice := unsafe.Slice(ports, length)
    config.HTTPPorts = make([]int, length)
    for i := 0; i < int(length); i++ {
        config.HTTPPorts[i] = int(slice[i])
    }
}

//export MiniTapSetHTTPS
func MiniTapSetHTTPS(ports *C.int, length C.int) {
    slice := unsafe.Slice(ports, length)
    config.HTTPSPorts = make([]int, length)
    for i := 0; i < int(length); i++ {
        config.HTTPSPorts[i] = int(slice[i])
    }
}

func DefaultInit() {
    if config.HTTPPorts  == nil {
        config.HTTPPorts = []int{80}
    }
    if config.HTTPSPorts == nil {
        config.HTTPSPorts = []int{443}
    }
    if config.Tun == "" {
        config.Tun = "mini-tun0"
    }
    if config.Subnet == "" {
        config.Subnet = "10.1.1.100/24"
    }
    if config.Gateway == "" {
    	config.Gateway = "10.1.1.1"
    }
}


func SetPingGroupRange() error {
    path := "/proc/sys/net/ipv4/ping_group_range"
    content := []byte("0 0\n")

    err := os.WriteFile(path, content, 0644)
    if err != nil {
        return fmt.Errorf("failed to write to %s: %w", path, err)
    }

    return nil
}



func RunNetwork() (int, error) {

	// Set PR_SET_PDEATHSIG to SIGKILL
	if err := unix.Prctl(unix.PR_SET_PDEATHSIG, uintptr(syscall.SIGKILL), 0, 0, 0); err != nil {
		log.Fatalf("prctl failed: %v", err)
	}
     

	runtime.LockOSThread()
	defer runtime.UnlockOSThread()

        DefaultInit()
	//// create a new network namespace
	if err := unix.Unshare(unix.CLONE_NEWNET); err != nil {
		return -1, fmt.Errorf("error creating network namespace: %w", err)
	}

        fd, err := tun.Open(config.Tun)
	if err != nil {
		return -1, fmt.Errorf("error creating tun device: %w", err)
	}

	// find the link for the device we just created
	link, err := netlink.LinkByName(config.Tun)
	if err != nil {
		return -1, fmt.Errorf("error finding link for new tun device %q: %w", config.Tun, err)
	}

	verbosef("tun device has MTU %d", link.Attrs().MTU)

	// bring the link up
	err = netlink.LinkSetUp(link)
	if err != nil {
		return -1, fmt.Errorf("error bringing up link for %q: %w", config.Tun, err)
	}

	// parse the subnet that we will assign to the interface within the namespace
        

	linksubnet, err := netlink.ParseIPNet(config.Subnet)
	if err != nil {
		return -1, fmt.Errorf("error parsing subnet: %w", err)
	}


	// assign the address we just parsed to the link, which will change the routing table
	err = netlink.AddrAdd(link, &netlink.Addr{
		IPNet: linksubnet,
	})
	if err != nil {
		return -1, fmt.Errorf("error assign address to tun device: %w", err)
	}

	// parse the subnet corresponding to all globally routable ipv4 addresses
	ip4Routable, err := netlink.ParseIPNet("0.0.0.0/0")
	if err != nil {
		return -1, fmt.Errorf("error parsing global subnet: %w", err)
	}

	// parse the subnet corresponding to all globally routable ipv6 addresses
	ip6Routable, err := netlink.ParseIPNet("2000::/3")
	if err != nil {
		return -1, fmt.Errorf("error parsing global subnet: %w", err)
	}

	// add a route that sends all ipv4 traffic going anywhere to the tun device
	err = netlink.RouteAdd(&netlink.Route{
		Dst:       ip4Routable,
		LinkIndex: link.Attrs().Index,
	})
	if err != nil {
		return -1, fmt.Errorf("error creating default ipv4 route: %w", err)
	}

	// add a route that sends all ipv6 traffic going anywhere to the tun device
	err = netlink.RouteAdd(&netlink.Route{
		Dst:       ip6Routable,
		LinkIndex: link.Attrs().Index,
	})
	if err != nil {
		verbosef("error creating default ipv6 route: %v, ignoring", err)
	}

	// find the loopback device
	loopback, err := netlink.LinkByName("lo")
	if err != nil {
		return -1, fmt.Errorf("error finding link for loopback device: %w", err)
	}

	// bring the link up
	err = netlink.LinkSetUp(loopback)
	if err != nil {
		return -1, fmt.Errorf("error bringing up link for loopback device: %w", err)
	}

	// the application-level thing is the mux, which distributes new connections according to patterns
	var mux mux

	// handle DNS queries by calling net.Resolve
	mux.HandleUDP(":53", func(conn net.Conn) {
		defer conn.Close()
		for {
			// allocate new buffer on each iteration for now because different handlers for each packet
			// are started asynchronously
			payload := make([]byte, link.Attrs().MTU)
			n, err := conn.Read(payload)
			if err == net.ErrClosed {
				verbose("UDP connection closed, exiting the read loop")
				break
			}
			if err != nil {
				verbosef("error reading udp packet with conn.ReadFrom: %v, ignoring", err)
				continue
			}

			verbosef("read a UDP packet with %d bytes", n)

			// handle the DNS query asynchronously
			go handleDNS(context.Background(), conn, payload)
		}
	})
	// listen for other TCP connections and proxy to the world
	mux.HandleTCP("*", func(conn net.Conn) {
		dst := conn.LocalAddr().String()
		proxyConn("tcp", dst, conn)
	})

	// listen for other UDP connections and proxy to the world
	mux.HandleUDP("*", func(conn net.Conn) {
		dst := conn.LocalAddr().String()
		proxyConn("udp", dst, conn)
	})


	// create the stack with udp and tcp protocols
	s := stack.New(stack.Options{
		NetworkProtocols:   []stack.NetworkProtocolFactory{ipv4.NewProtocol, ipv6.NewProtocol},
		TransportProtocols: []stack.TransportProtocolFactory{tcp.NewProtocol, udp.NewProtocol, icmp.NewProtocol4},
	})

	// create a link endpoint based on the TUN device
	endpoint, err := fdbased.New(&fdbased.Options{
                FDs: []int{fd},
		MTU: uint32(link.Attrs().MTU),
	})
	if err != nil {
		return -1, fmt.Errorf("error creating link from tun device file descriptor: %v", err)
	}

	// create the TCP forwarder, which accepts gvisor connections and notifies the mux
	const maxInFlight = 100 // maximum simultaneous connections
	tcpForwarder := tcp.NewForwarder(s, 0, maxInFlight, func(r *tcp.ForwarderRequest) {
		// remote address is the IP address of the subprocess
		// local address is IP address that the subprocess was trying to reach
		verbosef("at TCP forwarder: %v:%v => %v:%v",
			r.ID().RemoteAddress, r.ID().RemotePort,
			r.ID().LocalAddress, r.ID().LocalPort)

		// dispatch the request via the mux
		go mux.notifyTCP(&tcpRequest{r, new(waiter.Queue)})
	})

	// TODO: this UDP forwarder sometimes only ever processes one UDP packet, other times it keeps going... :/
	// create the UDP forwarder, which accepts UDP packets and notifies the mux
	udpForwarder := udp.NewForwarder(s, func(r *udp.ForwarderRequest) {
		// remote address is the IP address of the subprocess
		// local address is IP address that the subprocess was trying to reach
		verbosef("at UDP forwarder: %v:%v => %v:%v",
			r.ID().RemoteAddress, r.ID().RemotePort,
			r.ID().LocalAddress, r.ID().LocalPort)


		var wq waiter.Queue
		ep, err := r.CreateEndpoint(&wq)
		if err != nil {
			verbosef("error accepting connection: %v", err)
			return
		}

		// dispatch the request via the mux
		go mux.notifyUDP(gonet.NewUDPConn(&wq, ep))
	})

	// register the forwarders with the stack
	s.SetTransportProtocolHandler(tcp.ProtocolNumber, tcpForwarder.HandlePacket)
	s.SetTransportProtocolHandler(udp.ProtocolNumber, udpForwarder.HandlePacket)

	// create the network interface -- tun2socks says this must happen *after* registering the TCP forwarder
	nic := s.NextNICID()
	er := s.CreateNIC(nic, endpoint)
	if er != nil {
		return -1, fmt.Errorf("error creating NIC: %v", er)
	}

	// set promiscuous mode so that the forwarder receives packets not addressed to us
	er = s.SetPromiscuousMode(nic, true)
	if er != nil {
		return -1, fmt.Errorf("error activating promiscuous mode: %v", er)
	}

	// set spoofing mode so that we can send packets from any address
	er = s.SetSpoofing(nic, true)
	if er != nil {
		return -1, fmt.Errorf("error activating spoofing mode: %v", er)
	}

	// set up the route table so that we can send packets to the subprocess
	s.SetRouteTable([]tcpip.Route{
		{
			Destination: header.IPv4EmptySubnet,
			NIC:         nic,
		},
		{
			Destination: header.IPv6EmptySubnet,
			NIC:         nic,
		},
	})

        SetPingGroupRange()
        ExecuteFirewallRules()
      	ppid := syscall.Getppid();
        syscall.Kill(ppid, syscall.SIGUSR1)
        fmt.Println("Done with the config of tcp/ip");
        select {}

}


//export MiniTapUserTCPIP
func MiniTapUserTCPIP() C.int {
  res, err := RunNetwork()
  if err != nil {
    fmt.Println("RunNetwork error:", err)
    return -1;
  }
  return C.int(res);
}



func ParseArg(args []string) {
    var firewall_rules string

    if len(args) > 1 {
        argPath := args[1]
        if fileExists(argPath) {
            firewall_rules = argPath
        }
    }
    if firewall_rules == "" && fileExists("/tmp/firewall.rules") {
        firewall_rules = "/tmp/firewall.rules"
    }

    if firewall_rules == "" {
        fmt.Println("No valid firewall rules file found.")
        return
    }

    ReadFirewallRules(firewall_rules)
}


func main() {
	log.SetOutput(os.Stdout)
	log.SetFlags(0)
        ParseArg(os.Args)
	_, err := RunNetwork()
	if err != nil {
		// if we exit due to a subprocess returning with non-zero exit code then do not
		// print any extraneous output but do exit with the same code
		var exitError *exec.ExitError
		if errors.As(err, &exitError) {
			os.Exit(exitError.ExitCode())
		}

		// for any other kind of error, print the error and exit with code 1
		log.Fatal(err)
	}
}
