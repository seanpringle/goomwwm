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

winrulemap rulemap[] = {
	{ "tag1", TAG1 },
	{ "tag2", TAG2 },
	{ "tag3", TAG3 },
	{ "tag4", TAG4 },
	{ "tag5", TAG5 },
	{ "tag6", TAG6 },
	{ "tag7", TAG7 },
	{ "tag8", TAG8 },
	{ "tag9", TAG9 },
	{ "ignore", RULE_IGNORE },
	{ "above", RULE_ABOVE },
	{ "sticky", RULE_STICKY },
	{ "below", RULE_BELOW },
	{ "fullscreen", RULE_FULLSCREEN },
	{ "maximize_horz", RULE_MAXHORZ },
	{ "maximize_vert", RULE_MAXVERT },
	{ "top",    RULE_TOP },
	{ "bottom", RULE_BOTTOM },
	{ "left",   RULE_LEFT },
	{ "right",  RULE_RIGHT },
	{ "small",  RULE_SMALL },
	{ "medium", RULE_MEDIUM },
	{ "large",  RULE_LARGE },
	{ "cover", RULE_COVER },
	{ "steal", RULE_STEAL },
	{ "block", RULE_BLOCK },
	{ "hlock", RULE_HLOCK },
	{ "vlock", RULE_VLOCK },
	{ "expand", RULE_EXPAND },
	{ "contract", RULE_CONTRACT },
	{ "skip_taskbar", RULE_SKIPTBAR },
	{ "skip_pager", RULE_SKIPPAGE },
	{ "raise", RULE_RAISE },
	{ "lower", RULE_LOWER },
};

// load a rule specified on cmd line or .goomwwmrc
void rule_parse(char *rulestr)
{
	winrule *new = allocate_clear(sizeof(winrule));
	char *str = strdup(rulestr); strtrim(str);
	char *left = str, *right = str;
	// locate end of pattern
	while (*right && !isspace(*right)) right++;
	strncpy(new->pattern, str, right-left);
	while (*right && isspace(*right)) right++;
	// walk over rule flags, space or command delimited
	while (*right && !isspace(*right))
	{
		left = right;
		// scan for delimiters
		while (*right && !strchr(" ,\t", *right)) right++;
		if (right > left)
		{
			int i; for (i = 0; i < sizeof(rulemap)/sizeof(winrulemap); i++)
				if (!strncasecmp(left, rulemap[i].name, right-left))
					{ new->flags |= rulemap[i].flag; break; }
		}
		// skip delimiters
		while (*right && strchr(" ,\t", *right)) right++;
	}
	new->next = config_rules;
	config_rules = new;
	free(str);
}
