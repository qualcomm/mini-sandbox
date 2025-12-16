default: bin

bin:
	mkdir -p out/
	go build -o out/minitap .

test-unshare: default
	gcc test/unshare-bash.c test/utils.c -Itest/ -o test/unshare.bin
	test/unshare.bin wget google.com

test: test-unshare

clean: 
	rm -rf out/*
