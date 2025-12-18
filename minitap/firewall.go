/*
 * Copyright (c) 2025 Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: MIT
 */


package main

import (
	"bufio"
	"context"
	"fmt"
	"net"
	"os"
	"sync"
	"time"
	"github.com/miekg/dns"
)

type FirewallRules struct {
	Rules []string
	Count int
}

var (
	fwRules FirewallRules
	mu      sync.Mutex
)

var allowedIps =make(map[string]int)
var deniedDomainMap []string
var maxConnections int = -1
var connections int = 0

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

func InitFirewall() {
        // TODO make the argument flow to this point
        maxConnections = 5 
	if fwRules.Count == 0 {
		return
	}

	for i := fwRules.Count - 1; i >= 0; i-- {
		rule := fwRules.Rules[i]

		verbosef("Rule: %s,\n", rule)

		if net.ParseIP(rule) == nil {
			// Not a literal IP — either invalid or it's a domain name.
			domainMap[dns.Fqdn(rule)] = []string{}
		} else {
			allowedIps[rule]=1
		}

	}

	verbosef("AllowedIps: %s,\n", allowedIps)
	verbosef("DomainMap: %s,\n", domainMap)

}

func updateFirewall(domain_name string, ips []net.IP) {
	//domainMap is filled by InitFirewall with the allowed domain names. If a domain name is not already in
	var fully_qualified_domain_name = dns.Fqdn(domain_name)
	verbosef("updating firewall", fully_qualified_domain_name, ips)
	if _, ok := domainMap[fully_qualified_domain_name]; ok {
		for _, ip := range domainMap[fully_qualified_domain_name]{
			//we clean the previous ips for this domain
			delete(allowedIps,ip)
		}
		var ipStrs []string
  		for _, ip := range ips {
			if ip == nil { // skip nils
				continue
			}
			var ipString=ip.To4().String()
			ipStrs = append(ipStrs,ipString )
			allowedIps[ipString]=1
    	}	
		domainMap[fully_qualified_domain_name] = ipStrs
	} else {
		//we keep a list of the domains that the sandboxed payload tried to reach but are firewall'd.
		deniedDomainMap = append(deniedDomainMap, fully_qualified_domain_name)
		verbosef("deniedDomainMap %s", deniedDomainMap)
	}
}

func solveDomainName(domain_name string) {

	ctx, cancel := context.WithTimeout(context.Background(), 5*time.Second)
	defer cancel()
	var req dns.Msg
	req.SetQuestion(dns.Fqdn(domain_name), dns.TypeA)
	handleDNSQuery(ctx, &req)
}


func firewallConnection(addr net.Addr) bool {
	var destination_ip net.IP
	switch addr := addr.(type) {
	case *net.UDPAddr:
		destination_ip = addr.IP
	case *net.TCPAddr:
		destination_ip = addr.IP
	default:
		//We ignore connections that are not either TCP or UDP
		return false
	}
        fmt.Println("Inside firewallConnection\n\n")
	if fwRules.Count == 0 {
		if connections < maxConnections {
                        verbosef("connection number: %d, remaining: %d\n", connections, maxConnections);
                	connections ++;
			return true;
                }
		return false;
	}

	//Fast path,if we hit here, we don't need to query the dns again
	ip, ok := allowedIps[destination_ip.To4().String()]
	if ok { 
		verbosef("Hit ip: %s in allowedIps\n", ip)
		return true
	}else{
		//Slow path, we query dns and next time we use the fast path
		//We update all the domain names
		for domain := range domainMap {
			verbosef("Domain: %s\n", domain)
			solveDomainName(domain)
		}

		ip, ok := allowedIps[destination_ip.To4().String()]

		if ok { 
			verbosef("Hit ip: %s in allowedIps after dns query\n", ip)
			return true
		}
	}
	return false
}

func firewallDns(addr string) bool{
	_, ok := domainMap[dns.Fqdn(addr)]
	return ok
}
