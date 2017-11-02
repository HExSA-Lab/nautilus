VERSION = 2.1

SRC1 = actions.c link_list.c main.c symbol_table.c program.c vcode_table.c \
       rtstack.c stack.c vstack.c check_args.c cvl_table.c constant.c \
       vcode_hash.c io_nautilus.o

SRC2 = lex.yy.c y.tab.c

SRC3 = grammar.yy tokens.ll

OBJS = $(SRC1:.c=.o) $(SRC2:.c=.o) y.tab.o lex.yy.o

CFLAGS += -Iinclude/rt/nesl

obj-y += $(OBJS)


#
#
# The following rules need to be run explicitly
# before the main 
#
prebuild: lex.yy.c y.tab.c y.tab.h

prebuild-force:
	flex tokens.flex
	bison -y -d grammar.yy 

lex.yy.c : tokens.flex
	flex tokens.flex

#flex tokens.flex

y.tab.c y.tab.h:	grammar.yy
	bison -y -d grammar.yy 

