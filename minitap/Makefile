default: bin

bin:
	mkdir -p out/
	go build -o out/minitap .

test-firewall-rule: default
	gcc test/unshare-bash.c test/utils.c -Itest/ -o test/unshare.bin
	echo "-1" > /tmp/firewall.rules
	echo "www.google.com" >> /tmp/firewall.rules
	test/unshare.bin bash -c 'wget --timeout=1 --tries=1 www.google.com ; R1=$$? ; wget --timeout=1 --tries=1 www.qualcomm.com ; [ $$? -ne "$$R1" ] && echo "Only one connection worked! (Expected)" || echo "Something went wrong (Not Expected)" ' >> test.log

test-one-connection: default
	gcc test/unshare-bash.c test/utils.c -Itest/ -o test/unshare.bin
	echo "1" > /tmp/firewall.rules
	test/unshare.bin bash -c 'wget --timeout=1 --tries=1 www.google.com ; R1=$$? ; wget --timeout=1 --tries=1 www.google.com ; [ $$? -ne "$$R1" ] && echo "Only one connection worked! (Expected)" || echo "Something went wrong (Not Expected)" ' >> test.log

test-any-connection: default
	gcc test/unshare-bash.c test/utils.c -Itest/ -o test/unshare.bin
	echo "-1" > /tmp/firewall.rules
	test/unshare.bin bash -c 'wget --timeout=1 --tries=1 www.google.com ; R1=$$? ; wget --timeout=1 --tries=1 www.qualcomm.com ; [ $$? -eq 0 ] && [ $$R1 -eq 0 ] && echo "All connections worked (Expected)" || echo "One connection did not work! (Not expected)" ' >> test.log

init-log:
	rm -f test.log
	touch test.log

test: init-log test-firewall-rule test-one-connection test-any-connection

clean: 
	rm -rf out/*
	rm -f test.log
