package main

import (
	"net"
	"strings"
	"sync"
)

func patternMatches(pattern string, addr net.Addr) bool {
	if pattern == "*" {
		return true
	}
	if strings.HasPrefix(pattern, ":") && strings.HasSuffix(addr.String(), pattern) {
		return true
	}
	return false
}

// mux dispatches network connections to listeners according to patterns
type mux struct {
	mu          sync.Mutex
	tcpHandlers []*tcpMuxEntry
	udpHandlers []*udpMuxEntry
}

// tcpHandlerFunc is a function that receives TCP connections
type tcpHandlerFunc func(net.Conn)

// tcpHandlerFunc is a function that receives TCP connection requests and can choose
// whether to accept or reject them.
type tcpRequestHandlerFunc func(TCPRequest)

// tcpMuxEntry is a pattern and corresponding handler, for use in the mux table for the tcp stack
type tcpMuxEntry struct {
	pattern string
	handler tcpRequestHandlerFunc
}

// udpHandlerFunc is a function that receives UDP connections. We use net.Conn rather than
// net.PacketConn because the former matches io.Write and automatically sends the packet back
// to the source from which it came.
type udpHandlerFunc func(net.Conn)

// udpMuxEntry is a pattern and corresponding handler, for use in the mux table for the udp stack
type udpMuxEntry struct {
	handler udpHandlerFunc
	pattern string
}

// HandleTCP register a handler to be called each time a new connection is intercepted matching the
// given filter pattern.
//
// Pattern can a hostname, a :port, a hostname:port, or "*" for everything". For example:
//   - "example.com"
//   - "example.com:80"
//   - ":80"
//   - "*"
func (s *mux) HandleTCP(pattern string, handler tcpHandlerFunc) {
	s.HandleTCPRequest(pattern, func(r TCPRequest) {
		conn, err := r.Accept()
		if err != nil {
			errorf("error accepting connection: %v", err)
			return
		}
		handler(conn)
	})
}

// HandleTCPRequest register a handler to be called each time a new connection is intercepted matching the
// given filter pattern. Unlike HandleTCP, the handler can control whether the connection is accepted or
// rejected, which means replying with a SYN+ACK or SYN+RST respectively.
//
// Pattern can a hostname, a :port, a hostname:port, or "*" for everything". For example:
//   - "example.com"
//   - "example.com:80"
//   - ":80"
//   - "*"
func (s *mux) HandleTCPRequest(pattern string, handler tcpRequestHandlerFunc) {
	s.mu.Lock()
	defer s.mu.Unlock()

	s.tcpHandlers = append(s.tcpHandlers, &tcpMuxEntry{pattern: pattern, handler: handler})
}

// HandleUDP registers a handler for UDP packets according to destination IP and/or por
//
// Pattern can a hostname, a port, a hostname:port, or "*" for everything". Ports are prepended
// with colons. Valid patterns are:
//   - "example.com"
//   - "example.com:80"
//   - ":80"
//   - "*"
//
// Later this will be like net.Listen
func (s *mux) HandleUDP(pattern string, handler udpHandlerFunc) {
	s.mu.Lock()
	defer s.mu.Unlock()

	s.udpHandlers = append(s.udpHandlers, &udpMuxEntry{pattern: pattern, handler: handler})
}

// notifyTCP is called when a new stream is created. It finds the first listener
// that will accept the given stream. It never blocks.
func (s *mux) notifyTCP(req TCPRequest) {
	s.mu.Lock()
	defer s.mu.Unlock()

	for _, entry := range s.tcpHandlers {
		verbosef(" listening for tcp to %v", req.LocalAddr())
		if patternMatches(entry.pattern, req.LocalAddr()) &&  firewallConnection(req.LocalAddr()) {
			go entry.handler(req)
			return
		}
	}

	verbosef("nobody listening for tcp to %v, dropping", req.LocalAddr())
}

// notifyUDP is called when a new packet arrives. It finds the first handler
// with a pattern that matches the packet and delivers the packet to it
func (s *mux) notifyUDP(conn net.Conn) {
	s.mu.Lock()
	defer s.mu.Unlock()

	for _, entry := range s.udpHandlers {
		if patternMatches(entry.pattern, conn.LocalAddr()) &&  firewallConnection(conn.LocalAddr()) {
			go entry.handler(conn)
			return
		}
	}

	verbosef("nobody listening for udp to %v, dropping!", conn.LocalAddr())
}
