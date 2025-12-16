package main

import (
	"io"
	"net"
)

// proxyConn proxies data received on one TCP connection to the world, and back the other way.
func proxyConn(network, addr string, subprocess net.Conn) {
	// the connections's "LocalAddr" is actually the address that the other side (the subprocess) was trying
	// to reach, so that's the address we dial in order to proxy
	world, err := net.Dial(network, addr)
	if err != nil {
		// TODO: report errors not related to destination being unreachable
		subprocess.Close()
		return
	}

	verbosef(
		"subprocess type=%T local=%s remote=%s\n",
		subprocess,
		subprocess.LocalAddr(),
		subprocess.RemoteAddr(),
	)

	go proxyBytes(subprocess, world)
	go proxyBytes(world, subprocess)
}

// proxyBytes copies data between the world and the subprocess
func proxyBytes(w io.Writer, r io.Reader) {

	buf := make([]byte, 1<<20)
	for {
		n, err := r.Read(buf)
		if err == io.EOF {
			return
		}
		if err != nil {
			//Usually we end up in this branch if the process is killed before we complete the write (e.g. wget smth | head -n1)
			//We don't want to print the error verbosely so we use verbosef
			verbosef("error reading in proxyBytes: %v, abandoning", err)
			return
		}

		// send packet to channel, drop on failure
		_, err = w.Write(buf[:n])
		if err != nil {
			//Usually we end up in this branch if the process is killed before we complete the write (e.g. wget smth | head -n1)
			//We don't want to print the error verbosely so we use verbosef
			verbosef("error writing in proxyBytes: %s, dropping %d bytes", err, n)
		}
	}
}
