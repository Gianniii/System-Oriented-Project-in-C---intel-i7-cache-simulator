## ======================================================================
## partial Makefile provided to students
##

CFLAGS = -std=c11 -Wall -Wpedantic -g

# a bit more checks if you'd like to (uncomment
# CFLAGS += -Wextra -Wfloat-equal -Wshadow                         \
# -Wpointer-arith -Wbad-function-cast -Wcast-align -Wwrite-strings \
# -Wconversion -Wunreachable-code

# uncomment if you want to add DEBUG flag
CPPFLAGS += -DDEBUG

# ----------------------------------------------------------------------
# feel free to update/modifiy this part as you wish

# all those libs are required on Debian, feel free to adapt it to your box

# Managing make on different OSs
# From StackOverflow https://stackoverflow.com/questions/714100/os-detecting-makefile
ifeq ($(OS),Windows_NT)
    # Shouldn't happen
else
    UNAME_S := $(shell uname -s)
    ifeq ($(UNAME_S),Linux) # LINUX
      LDLIBS += -lcheck -lm -lrt -pthread -lsubunit
    endif
    ifeq ($(UNAME_S),Darwin) # MACOSX
			LDLIBS += -lcheck -lm -pthread
    endif
endif

# TODO DONT FORGET to put back   test-addr
all:: test-commands test-memory test-tlb_simple test-tlb_hrchy #test-addr

error.o: error.h error.c

addr_mng.o: addr_mng.c addr_mng.h error.h addr.h

commands.o: addr.h mem_access.h addr_mng.h error.h commands.h commands.c

page_walk.o : page_walk.c commands.h error.h addr_mng.h addr.h

memory.o: memory.c memory.h error.h addr_mng.h util.h error.h addr.h

tlb_hrchy_mng.o: tlb_hrchy.h mem_access.h addr.h page_walk.h

test-addr.o: test-addr.c tests.h util.h addr.h addr_mng.h
test-addr: test-addr.o addr_mng.o test-addr.o

test-commands.o: test-commands.c commands.h error.h
test-commands: test-commands.o commands.o error.o addr_mng.o

test_memory.o: test-memory.c error.h memory.h page_walk.h addr_mng.h util.h
test-memory: test-memory.o error.o memory.o page_walk.o addr_mng.o

list.o: list.c list.h error.h

tlb_mng.o : tlb_mng.c tlb.h addr.h list.h addr_mng.h error.h tlb_mng.h page_walk.h util.h

test-tlb_simple.o: test-tlb_simple.c list.h error.h util.h addr_mng.h commands.h memory.h tlb.h tlb_mng.h
test-tlb_simple: test-tlb_simple.o error.o list.o addr_mng.o commands.o memory.o tlb_mng.o page_walk.o

test-tlb_hrchy.o: error.h util.h addr_mng.h commands.h memory.h tlb_hrchy.h tlb_hrchy_mng.h page_walk.h
test-tlb_hrchy: error.o addr_mng.o commands.o memory.o tlb_hrchy_mng.o page_walk.o

# ----------------------------------------------------------------------
# This part is to make your life easier. See handouts how to make use of it.

clean::
	-@/bin/rm -f *.o *~ $(CHECK_TARGETS)

new: clean all

static-check:
	scan-build -analyze-headers --status-bugs -maxloop 64 make CC=clang new

style:
	astyle -n -o -A8 *.[ch]

# all those libs are required on Debian, adapt to your box
$(CHECK_TARGETS): LDLIBS += -lcheck -lm -lrt -pthread -lsubunit

check:: $(CHECK_TARGETS)
	$(foreach target,$(CHECK_TARGETS),./$(target);)

# target to run tests
check:: all
	@if ls tests/*.*.sh 1> /dev/null 2>&1; then \
	  for file in tests/*.*.sh; do [ -x $$file ] || echo "Launching $$file"; ./$$file || exit 1; done; \
	fi

IMAGE=arashpz/feedback:week09
feedback:
	@docker pull $(IMAGE)
	@docker run -it --rm -v ${PWD}:/home/tester/done $(IMAGE)

SUBMIT_SCRIPT=../provided/submit.sh
submit1: $(SUBMIT_SCRIPT)
	@$(SUBMIT_SCRIPT) 1

submit2: $(SUBMIT_SCRIPT)
	@$(SUBMIT_SCRIPT) 2

submit:
	@printf 'what "make submit"??\nIt'\''s either "make submit1" or "make submit2"...\n'
