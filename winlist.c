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

winlist* winlist_new()
{
	winlist *l = allocate(sizeof(winlist)); l->len = 0;
	l->array = allocate(sizeof(Window) * (WINLIST+1));
	l->data  = allocate(sizeof(void*) * (WINLIST+1));
	return l;
}

int winlist_append(winlist *l, Window w, void *d)
{
	if (l->len > 0 && !(l->len % WINLIST))
	{
		l->array = reallocate(l->array, sizeof(Window) * (l->len+WINLIST+1));
		l->data  = reallocate(l->data,  sizeof(void*)  * (l->len+WINLIST+1));
	}
	l->data[l->len] = d;
	l->array[l->len++] = w;
	return l->len-1;
}

void winlist_prepend(winlist *l, Window w, void *d)
{
	winlist_append(l, None, NULL);
	memmove(&l->array[1], &l->array[0], sizeof(Window) * (l->len-1));
	memmove(&l->data[1],  &l->data[0],  sizeof(void*)  * (l->len-1));
	l->array[0] = w;
	l->data[0] = d;
}

void winlist_empty(winlist *l)
{
	while (l->len > 0) free(l->data[--(l->len)]);
}

void winlist_free(winlist *l)
{
	winlist_empty(l); free(l->array); free(l->data); free(l);
}

int winlist_find(winlist *l, Window w)
{
	// iterate backwards. theory is: windows most often accessed will be
	// nearer the end. testing with kcachegrind seems to support this...
	int i; Window o; winlist_descend(l, i, o) if (w == o) return i;
	return -1;
}

int winlist_forget(winlist *l, Window w)
{
	int i, j;
	for (i = 0, j = 0; i < l->len; i++, j++)
	{
		l->array[j] = l->array[i];
		l->data[j]  = l->data[i];
		if (l->array[i] == w) { free(l->data[i]); j--; }
	}
	l->len -= (i-j);
	return j != i ?1:0;
}

void winlist_reverse(winlist *l)
{
	int i, j;
	for (i = 0, j = l->len-1; i < j; i++, j--)
	{
		Window w = l->array[i]; void *d = l->data[i];
		l->array[i] = l->array[j]; l->data[i] = l->data[j];
		l->array[j] = w; l->data[j] = d;
	}
}
