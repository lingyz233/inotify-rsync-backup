TARGET=a.out
C_SRC=$(wildcard *.c)
C_OBJ=$(patsubst %.c,%.o,$(C_SRC))

CFLAGS+=-O3 -mavx2 -mfma

PKG_LIB=pkg-config --libs 
PKG_CF=pkg-config --cflags 

LUA_LIBS=$(shell $(PKG_LIB) lua5.4)
LUA_CFLAGS=$(shell $(PKG_CF) lua5.4)

GLIB_LIBS=$(shell $(PKG_LIB) glib-2.0)
GLIB_CFLAGS=$(shell $(PKG_CF) glib-2.0)

.PHONY : all clean

all : $(TARGET)
	@echo DONE

$(TARGET) : $(C_OBJ)
	gcc $^ -o $@ $(LUA_LIBS) $(LUA_CFLAGS) $(GLIB_LIBS) $(GLIB_CFLAGS) $(CFLAGS)

$(C_OBJ) : %.o : %.c
	gcc -c $^ $(LUA_CFLAGS) $(GLIB_CFLAGS) $(CFLAGS)

clean :
	rm -rf $(TARGET) *.o
