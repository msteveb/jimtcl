JIMSH ?= ../jimsh

test: $(JIMSH)
	@for i in *.test; do $(JIMSH) $$i; done

../jimsh: ../*.c
	make -C .. all
