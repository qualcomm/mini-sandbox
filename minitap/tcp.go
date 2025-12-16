package main

import (
	"fmt"
	"net"
	"gvisor.dev/gvisor/pkg/tcpip/adapters/gonet"
	"gvisor.dev/gvisor/pkg/tcpip/transport/tcp"
	"gvisor.dev/gvisor/pkg/waiter"
)



// // TCPRequest represents a request by a remote host to initiate a TCP connection. The interface provides
// // a way for the application to decide whether or not to accept the connection.
type TCPRequest interface {
	// RemoteAddr is the IP address and port of the subprocess that initiated the connection
	RemoteAddr() net.Addr

	// LocalAddr is the IP address and port that the subprocess was trying to reach
	LocalAddr() net.Addr

	// Accept replies with a SYN+ACK back and returns a connection that can be read
	Accept() (net.Conn, error)

	// Reject replies with a RST and the connection is done
	Reject()
}

type tcpRequest struct {
	fr *tcp.ForwarderRequest
	wq *waiter.Queue
}

func (r *tcpRequest) RemoteAddr() net.Addr {
	addr := r.fr.ID().RemoteAddress
	return &net.TCPAddr{IP: addr.AsSlice(), Port: int(r.fr.ID().RemotePort)}
}

func (r *tcpRequest) LocalAddr() net.Addr {
	addr := r.fr.ID().LocalAddress
	return &net.TCPAddr{IP: addr.AsSlice(), Port: int(r.fr.ID().LocalPort)}
}

func (r *tcpRequest) Accept() (net.Conn, error) {
	ep, err := r.fr.CreateEndpoint(r.wq)
	if err != nil {
		r.fr.Complete(true)
		return nil, fmt.Errorf("CreateEndpoint: %v", err)
	}

// 	// TODO: set keepalive count, keepalive interval, receive buffer size, send buffer size, like this:
// 	//   https://github.com/xjasonlyu/tun2socks/blob/main/core/tcp.go#L83

	// create an adapter that makes a gvisor endpoint into a net.Conn
	conn := gonet.NewTCPConn(r.wq, ep)
	r.fr.Complete(false)
	return conn, nil
}

func (r *tcpRequest) Reject() {
	r.fr.Complete(true)
}
