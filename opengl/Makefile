TARGET = render-512
LDFLAGS = -lglut -lGLU -lGL

$(TARGET): render_gl_points.c
	@echo "CC $<"
	@$(CC) -o $@ $<  $(LDFLAGS)
	@echo "LD $@"

all: $(TARGET) 

clean:
	@rm -f $(TARGET) 2>&1 >/dev/null

