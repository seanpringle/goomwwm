/* GoomwWM, Get out of my way, Window Manager!

MIT/X11 License
Copyright (c) 2012 Sean Pringle <sean.pringle@gmail.com>

Permission is hereby granted, free of charge, to any person obtaining
a copy of this software and associated documentation files (the
"Software"), to deal in the Software without restriction, including
without limitation the rights to use, copy, modify, merge, publish,
distribute, sublicense, and/or sell copies of the Software, and to
permit persons to whom the Software is furnished to do so, subject to
the following conditions:

The above copyright notice and this permission notice shall be
included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

*/

void* allocate(unsigned long bytes)
{
	void *ptr = malloc(bytes);
	if (!ptr)
	{
		fprintf(stderr, "malloc failed!\n");
		exit(EXIT_FAILURE);
	}
	return ptr;
}

void* allocate_clear(unsigned long bytes)
{
	void *ptr = allocate(bytes);
	memset(ptr, 0, bytes);
	return ptr;
}

void* reallocate(void *ptr, unsigned long bytes)
{
	ptr = realloc(ptr, bytes);
	if (!ptr)
	{
		fprintf(stderr, "realloc failed!\n");
		exit(EXIT_FAILURE);
	}
	return ptr;
}

// trim string in place
void strtrim(char *str)
{
	int i = 0, j = 0;
	while (isspace(str[i])) i++;
	while (str[i]) str[i++] = str[j++];
	while (isspace(str[--j]));
	str[++j] = '\0';
}

double timestamp()
{
	struct timeval tv; gettimeofday(&tv, NULL);
	return tv.tv_sec + (double)tv.tv_usec/1000000;
}

void catch_exit(int sig)
{
	while (0 < waitpid(-1, NULL, WNOHANG));
}

// execute sub-process
pid_t exec_cmd(char *cmd)
{
	if (!cmd || !cmd[0]) return -1;
	signal(SIGCHLD, catch_exit);
	pid_t pid = fork();
	if (!pid)
	{
		setsid();
		execlp("/bin/sh", "sh", "-c", cmd, NULL);
		exit(EXIT_FAILURE);
	}
	return pid;
}

// cli arg handling
int find_arg(int argc, char *argv[], char *key)
{
	int i; for (i = 0; i < argc && strcasecmp(argv[i], key); i++);
	return i < argc ? i: -1;
}

char* find_arg_str(int argc, char *argv[], char *key, char* def)
{
	int i = find_arg(argc, argv, key);
	return (i > 0 && i < argc-1) ? argv[i+1]: def;
}

int find_arg_opts(int argc, char *argv[], char *key, char **list, int count)
{
	char *s = find_arg_str(argc, argv, key, NULL);
	int i; for (i = 0; i < count; i++) if (s && !strcasecmp(s, list[i])) return i;
	return -1;
}

int find_arg_int(int argc, char *argv[], char *key, int def)
{
	int i = find_arg(argc, argv, key);
	return (i > 0 && i < argc-1) ? strtol(argv[i+1], NULL, 10): def;
}

// true if keysym exists in array
int in_array_keysym(KeySym *array, KeySym code)
{
	int i; for (i = 0; array[i]; i++)
		if (array[i] == code) return i;
	return -1;
}

// allocate a pixel value for an X named color
unsigned int color_get(Display *d, const char *name)
{
	XColor color;
	Colormap map = DefaultColormap(d, DefaultScreen(d));
	return XAllocNamedColor(d, map, name, &color, &color) ? color.pixel: None;
}

// find mouse pointer location
int pointer_get(Window root, int *x, int *y)
{
	*x = 0; *y = 0;
	Window rr, cr; int rxr, ryr, wxr, wyr; unsigned int mr;
	if (XQueryPointer(display, root, &rr, &cr, &rxr, &ryr, &wxr, &wyr, &mr))
	{
		*x = rxr; *y = ryr;
		return 1;
	}
	return 0;
}

// true if a keycode matches one of our modkeys
int keycode_is_mod(unsigned int code)
{
	int k; for (k = 0; config_modkeycodes[k]; k++)
		if (config_modkeycodes[k] == code)
			return 1;
	return 0;
}

// check whether our modkeys are currently pressed
int modkey_is_down()
{
	char keys[32];	int i, j;
	XQueryKeymap(display, keys);
	for (i = 0; i < 32; i++)
	{
		if (!keys[i]) continue;
		unsigned int bits = keys[i];
		for (j = 0; j < 8; j++)
			if (bits & 1<<j && keycode_is_mod((i*8)+j))
				return 1;
	}
	return 0;
}

#ifdef DEBUG
void event_log(const char *e, Window w)
{
	XClassHint chint;
	fprintf(stderr, "\n%s: %x", e, (unsigned int)w);
	if (w != None && XGetClassHint(display, w, &chint))
	{
		fprintf(stderr, " %s", chint.res_class);
		XFree(chint.res_class); XFree(chint.res_name);
	}
	fprintf(stderr, "\n");
	fflush(stderr);
}
#else
#define event_log(...)
#endif

#ifdef DEBUG
void event_note(const char *fmt, ...)
{
	fprintf(stderr, "\t");
	va_list ap; va_start(ap,fmt); vfprintf(stderr, fmt, ap); va_end(ap);
	fprintf(stderr, "\n");
}
#else
#define event_note(...)
#endif
