CFLAGS+= -Wall -pedantic -g -ggdb
CFLAGS+= -L .. -Wl,-R.. -I ..
CFLAGS+= -ljim

EXAMPLES=		\
	jim_command	\
	jim_hello	\
	jim_inline	\
	jim_list	\
	jim_obj		\
	jim_return

all: libjim.so $(EXAMPLES)

libjim.so:
	@echo  -----------------------------------------------------------
	@echo " Remember to call \"make libjim\" before trying our examples"
	@echo  -----------------------------------------------------------
	cp ../libjim.so libjim.so	

clean:
	rm -rf $(EXAMPLES)
	rm -rf *.core
	rm -rf libjim.so
