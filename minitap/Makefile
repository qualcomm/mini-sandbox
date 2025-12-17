default: bin

bin:
	mkdir -p out/
	go build -o out/minitap .

test-unshare: default
	gcc test/unshare-bash.c test/utils.c -Itest/ -o test/unshare.bin
	echo "www.google.com" > /tmp/firewall.rules
	test/unshare.bin wget www.google.com

test: test-unshare

clean: 
	rm -rf out/*
