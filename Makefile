TARGET = vaap-jpeg-decode
LDFLAGS = -lva -lva-x11 -lX11

$(TARGET): jpegdec.c
	@echo "CC $<"
	@$(CC) -o $@ $<  $(LDFLAGS)

all: $(TARGET) 

clean:
	@rm -f $(TARGET) 2>&1 >/dev/null

