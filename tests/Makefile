test: ../jimsh
	@for i in *.test; do ../jimsh $$i; done

../jimsh: ../*.c
	make -C .. all
