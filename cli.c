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

void cli_message(Atom atom, char *cmd)
{
	Window root = DefaultRootWindow(display);
	Window cli = XCreateSimpleWindow(display, root, 0, 0, 1, 1, 0, None, None);
	if (cmd) window_set_text_prop(cli, gatoms[GOOMWWM_MESSAGE], cmd);
	window_send_message(root, cli, atom, 0, SubstructureNotifyMask | SubstructureRedirectMask);
}

// command line interface
int cli_main(int argc, char *argv[])
{
	char *arg;

	if ((arg = find_arg_str(argc, argv, "-log", NULL)))
		cli_message(gatoms[GOOMWWM_LOG], arg);

	if (find_arg(argc, argv, "-restart") >= 0)
		cli_message(gatoms[GOOMWWM_RESTART], argv[0]);

	if ((arg = find_arg_str(argc, argv, "-exec", NULL)))
		cli_message(gatoms[GOOMWWM_RESTART], arg);

	if (find_arg(argc, argv, "-quit") >= 0)
		cli_message(gatoms[GOOMWWM_QUIT], NULL);


	//TODO: make this a two-way event exchange
	usleep(300000); // 0.3s
	return EXIT_SUCCESS;
}