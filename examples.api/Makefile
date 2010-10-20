CFLAGS+= -Wall -g
CFLAGS+= -I..
LDLIBS += -L.. -ljim

EXAMPLES=		\
	jim_command	\
	jim_hello	\
	jim_list	\
	jim_obj		\
	jim_return

all: $(EXAMPLES)

clean:
	rm -rf $(EXAMPLES)
	rm -rf *.core
