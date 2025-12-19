package main

import (
	"bufio"
	"context"
	"fmt"
	"io"
	"net"
	"os"
	"strings"

	"github.com/miekg/dns"
)

var domainMap = make(map[string][]string)

// dnsCall is a summary of a dns request/response exposed to the application level observers
type dnsCall struct {
	queries []dnsQuery
}

// a dnsQuery is a query/response pair
type dnsQuery interface {
	Type() string
	Query() string
	Answers() []string
}

// dnsPairA is an A or AAAA query together with responses
type dnsPairA struct {
	typ     uint16
	query   string
	answers []net.IP
}

func (p dnsPairA) Type() string {
	return dnsTypeCode(p.typ)
}

func (p dnsPairA) Query() string {
	return p.query
}

func (p dnsPairA) Answers() []string {
	var answers []string
	for _, a := range p.answers {
		answers = append(answers, a.String())
	}
	return answers
}

// handle a DNS query payload here is the application-level UDP payload
func handleDNS(ctx context.Context, w io.Writer, payload []byte) {
	var req dns.Msg
	err := req.Unpack(payload)
	if err != nil {
		errorf("error unpacking dns packet: %v, ignoring", err)
		return
	}

	if req.Opcode != dns.OpcodeQuery {
		errorf("ignoring a dns query with non-query opcode (%v)", req.Opcode)
		return
	}
	verbosef("DNS query  sending a response with empty answer %s", req)

	// resolve the query
	rrs, err := handleDNSQuery(ctx, &req)
	if err != nil {
		verbosef("DNS query returned: %v, sending a response with empty answer", err)
		// do not abort here, continue on and send a reply with no answer
	}

	resp := new(dns.Msg)
	resp.SetReply(&req)
	resp.Answer = rrs

	// serialize the response
	buf, err := resp.Pack()
	if err != nil {
		errorf("error serializing dns response: %v, abandoning...", err)
		return
	}

	// always send the entire buffer in a single Write() since UDP writes one packet per call to Write()
	verbosef("responding to DNS request with %d bytes...", len(buf))
	_, err = w.Write(buf)
	if err != nil {
		errorf("error writing dns response: %v, abandoning...", err)
		return
	}
}

func dnsTypeCode(t uint16) string {
	switch t {
	case dns.TypeNone:
		return "<None>"
	case dns.TypeA:
		return "A"
	case dns.TypeNS:
		return "NS"
	case dns.TypeMD:
		return "MD"
	case dns.TypeMF:
		return "MF"
	case dns.TypeCNAME:
		return "CNAME"
	case dns.TypeSOA:
		return "SOA"
	case dns.TypeMB:
		return "MB"
	case dns.TypeMG:
		return "MG"
	case dns.TypeMR:
		return "MR"
	case dns.TypeNULL:
		return "NULL"
	case dns.TypePTR:
		return "PTR"
	case dns.TypeHINFO:
		return "HINFO"
	case dns.TypeMINFO:
		return "MINFO"
	case dns.TypeMX:
		return "MX"
	case dns.TypeTXT:
		return "TXT"
	case dns.TypeRP:
		return "RP"
	case dns.TypeAFSDB:
		return "AFSDB"
	case dns.TypeX25:
		return "X25"
	case dns.TypeISDN:
		return "ISDN"
	case dns.TypeRT:
		return "RT"
	case dns.TypeNSAPPTR:
		return "NSAPPTR"
	case dns.TypeSIG:
		return "SIG"
	case dns.TypeKEY:
		return "KEY"
	case dns.TypePX:
		return "PX"
	case dns.TypeGPOS:
		return "GPOS"
	case dns.TypeAAAA:
		return "AAAA"
	case dns.TypeLOC:
		return "LOC"
	case dns.TypeNXT:
		return "NXT"
	case dns.TypeEID:
		return "EID"
	case dns.TypeNIMLOC:
		return "NIMLOC"
	case dns.TypeSRV:
		return "SRV"
	case dns.TypeATMA:
		return "ATMA"
	case dns.TypeNAPTR:
		return "NAPTR"
	case dns.TypeKX:
		return "KX"
	case dns.TypeCERT:
		return "CERT"
	case dns.TypeDNAME:
		return "DNAME"
	case dns.TypeOPT:
		return "OPT"
	case dns.TypeAPL:
		return "APL"
	case dns.TypeDS:
		return "DS"
	case dns.TypeSSHFP:
		return "SSHFP"
	case dns.TypeIPSECKEY:
		return "IPSECKEY"
	case dns.TypeRRSIG:
		return "RRSIG"
	case dns.TypeNSEC:
		return "NSEC"
	case dns.TypeDNSKEY:
		return "DNSKEY"
	case dns.TypeDHCID:
		return "DHCID"
	case dns.TypeNSEC3:
		return "NSEC3"
	case dns.TypeNSEC3PARAM:
		return "NSEC3PARAM"
	case dns.TypeTLSA:
		return "TLSA"
	case dns.TypeSMIMEA:
		return "SMIMEA"
	case dns.TypeHIP:
		return "HIP"
	case dns.TypeNINFO:
		return "NINFO"
	case dns.TypeRKEY:
		return "RKEY"
	case dns.TypeTALINK:
		return "TALINK"
	case dns.TypeCDS:
		return "CDS"
	case dns.TypeCDNSKEY:
		return "CDNSKEY"
	case dns.TypeOPENPGPKEY:
		return "OPENPGPKEY"
	case dns.TypeCSYNC:
		return "CSYNC"
	case dns.TypeZONEMD:
		return "ZONEMD"
	case dns.TypeSVCB:
		return "SVCB"
	case dns.TypeHTTPS:
		return "HTTPS"
	case dns.TypeSPF:
		return "SPF"
	case dns.TypeUINFO:
		return "UINFO"
	case dns.TypeUID:
		return "UID"
	case dns.TypeGID:
		return "GID"
	case dns.TypeUNSPEC:
		return "UNSPEC"
	case dns.TypeNID:
		return "NID"
	case dns.TypeL32:
		return "L32"
	case dns.TypeL64:
		return "L64"
	case dns.TypeLP:
		return "LP"
	case dns.TypeEUI48:
		return "EUI48"
	case dns.TypeEUI64:
		return "EUI64"
	case dns.TypeNXNAME:
		return "NXNAME"
	case dns.TypeURI:
		return "URI"
	case dns.TypeCAA:
		return "CAA"
	case dns.TypeAVC:
		return "AVC"
	case dns.TypeAMTRELAY:
		return "AMTRELAY"
	case dns.TypeTKEY:
		return "TKEY"
	case dns.TypeTSIG:
		return "TSIG"
	case dns.TypeIXFR:
		return "IXFR"
	case dns.TypeAXFR:
		return "AXFR"
	case dns.TypeMAILB:
		return "MAILB"
	case dns.TypeMAILA:
		return "MAILA"
	case dns.TypeANY:
		return "ANY"
	case dns.TypeTA:
		return "TA"
	case dns.TypeDLV:
		return "DLV"
	case dns.TypeReserved:
		return "Reserved"
	default:
		return fmt.Sprintf("unknown(%d)", t)
	}
}

var upstreamDNS string

func ReadFirstDNSServerWithPort() (string, error) {
	file, err := os.Open("/etc/resolv.conf")
	if err != nil {
		return "", fmt.Errorf("failed to open /etc/resolv.conf: %w", err)
	}
	defer file.Close()

	scanner := bufio.NewScanner(file)
	for scanner.Scan() {
		line := scanner.Text()
		if strings.HasPrefix(line, "nameserver") {
			fields := strings.Fields(line)
			if len(fields) >= 2 {
				return fields[1] + ":53", nil
			}
		}
	}

	if err := scanner.Err(); err != nil {
		return "", fmt.Errorf("error reading /etc/resolv.conf: %w", err)
	}

	return "", fmt.Errorf("no nameserver found in /etc/resolv.conf")
}

func setUpstreamDNS() {
	if upstreamDNS == "" {
		dns, err := ReadFirstDNSServerWithPort()
		if err != nil {
			fmt.Println("Warning: could not read DNS from resolv.conf, using default.")
			upstreamDNS = "8.8.8.8:53"
		} else {
			upstreamDNS = dns
		}
		verbosef("Using %s as DNS", upstreamDNS)
		if host, _, err := net.SplitHostPort(upstreamDNS); err == nil {
			allowedIps[host] = 1
		}
	}
}

// handleDNSQuery answers DNS queries according to:
//
//	net.DefaultResolver if the DNS request is A or AAAA
//	cloudflare DNS for other DNS requests
//

func handleDNSQuery(ctx context.Context, req *dns.Msg) ([]dns.RR, error) {

	if len(req.Question) == 0 {
		return nil, nil // this means no answer, no error, which is fine
	}

	question := req.Question[0]
	questionType := dnsTypeCode(question.Qtype)
	verbosef("got dns request for %v (%v)", question.Name, questionType)

	// the DNS call will be sent to watchers later
	var call dnsCall

	// handle the request ourselves
	switch question.Qtype {
	case dns.TypeA:
		var ips []net.IP
		var err error
		if !firewallDns(question.Name) {
			return nil, fmt.Errorf("Request denied by custom firewall")
		}
		ips, err = net.DefaultResolver.LookupIP(ctx, "ip4", question.Name)
		if err != nil {
			return nil, fmt.Errorf("for an A record the default resolver said: %w", err)
		}
		call.queries = append(call.queries, dnsPairA{
			typ:     dns.TypeA,
			query:   question.Name,
			answers: ips,
		})

		verbosef("resolved %v to %v with default resolver", question.Name, ips)

		var rrs []dns.RR
		for _, ip := range ips {
			rr, err := dns.NewRR(fmt.Sprintf("%s A %s", question.Name, ip))
			if err != nil {
				return nil, fmt.Errorf("error constructing rr: %w", err)
			}
			rrs = append(rrs, rr)
		}
		updateFirewall(question.Name, ips)

		return rrs, nil
	}
	return nil, fmt.Errorf("Not an A request")
}
