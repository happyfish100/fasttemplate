.SUFFIXES: .c .lo

COMPILE = $(CC)  -Wall -D_FILE_OFFSET_BITS=64 -D_GNU_SOURCE -g -O1 -DDEBUG_FLAG -fPIC
INC_PATH = -I/usr/include -I/usr/local/include
LIB_PATH =  -lfastcommon 
TARGET_LIB = /usr/local/lib

ALL_HEADERS = fast_template.h

SHARED_OBJS = fast_template.lo

ALL_OBJS = $(SHARED_OBJS)

ALL_LIBS = libfasttemplate.so

all: $(ALL_OBJS) $(ALL_LIBS) template_test

libfasttemplate.so: $(SHARED_OBJS)
	cc -shared -o $@ $^ $(LIB_PATH)

.lo:
	$(COMPILE) -o $@ $<  $(SHARED_OBJS) $(LIB_PATH) $(INC_PATH)
.c:
	$(COMPILE) -o $@ $<  $(ALL_OBJS) $(LIB_PATH) $(INC_PATH)
.c.lo:
	$(COMPILE) -c -o $@ $<  $(INC_PATH)
install:
	mkdir -p $(TARGET_LIB)
	mkdir -p /usr/local/lib
	mkdir -p /usr/local/include/fasttemplate

	install -m 755 $(ALL_LIBS) $(TARGET_LIB)
	cp -f $(ALL_HEADERS) /usr/local/include/fasttemplate/

	if [ ! -e /usr/lib/libfasttemplate.so ]; then ln -sf $(TARGET_LIB)/libfasttemplate.so /usr/lib/libfasttemplate.so; fi
	if [ -d /usr/lib64 ] && [ ! -e /usr/lib64/libfasttemplate.so ]; then ln -sf $(TARGET_LIB)/libfasttemplate.so /usr/lib64/libfasttemplate.so; fi
clean:
	rm -f *.lo $(ALL_LIBS) $(ALL_PRGS)

