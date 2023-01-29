CC := clang

TARGET_EXECUTABLE := si3s

TARGETS := ${TARGET_EXECUTABLE}

UBSAN_CHECKS := undefined,bounds
UBSAN_FLAGS := -fsanitize=${UBSAN_CHECKS} -fsanitize-minimal-runtime -fsanitize-trap=${UBSAN_CHECKS}

CFLAGS += -fPIC -Werror -Weverything -Wno-disabled-macro-expansion -Wno-padded -Wno-unused-macros -Wno-declaration-after-statement
CFLAGS += -Wno-reserved-id-macro -Wno-documentation-deprecated-sync
CFLAGS += ${UBSAN_FLAGS}
CFLAGS += -I/usr/local/include
LDFLAGS += -L/usr/local/lib
LDFLAGS += ${UBSAN_FLAGS}
LDLIBS += -ljson-c

build: ${TARGETS}
.PHONY: build

clean:
	rm -f *.o .depend *.core core
	rm -f ${TARGETS}
.PHONY: clean

${TARGET_EXECUTABLE}: main.o
	${CC} ${LDFLAGS} ${.ALLSRC} -o ${.TARGET} ${LDLIBS}
