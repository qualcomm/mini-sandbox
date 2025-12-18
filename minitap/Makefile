default: bin

bin:
	mkdir -p out/
	go build -o out/minitap .

test-firewall-rule: default
	gcc test/unshare-bash.c test/utils.c -Itest/ -o test/unshare.bin
	echo "-1" > /tmp/firewall.rules
	echo "www.google.com" >> /tmp/firewall.rules
	test/unshare.bin bash -c 'wget --timeout=1 --tries=1 www.google.com ; R1=$$? ; wget --timeout=1 --tries=1 www.qualcomm.com ; [ $$? -ne "$$R1" ] && echo "Only one connection worked! (Expected)" || echo "Something went wrong (Not Expected)" '


test-one-connection: default
	gcc test/unshare-bash.c test/utils.c -Itest/ -o test/unshare.bin
	echo "1" > /tmp/firewall.rules
	test/unshare.bin bash -c 'wget --timeout=1 --tries=1 www.google.com ; R1=$$? ; wget --timeout=1 --tries=1 www.google.com ; [ $$? -ne "$$R1" ] && echo "Only one connection worked! (Expected)" || echo "Something went wrong (Not Expected)" '

test-any-connection: default
	gcc test/unshare-bash.c test/utils.c -Itest/ -o test/unshare.bin
	echo "-1" > /tmp/firewall.rules
	test/unshare.bin bash -c 'wget --timeout=1 --tries=1 www.google.com ; R1=$$? ; wget --timeout=1 --tries=1 www.qualcomm.com ; [ $$? -ne "$$R1" ] && echo "One connection did not work! (Not expected)" || echo "All connections worked (Expected)" '

test: test-firewall-rule test-one-connection test-any-connection

clean: 
	rm -rf out/*
