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
	{ "snap_left", RULE_SNAPLEFT },
	{ "snap_right", RULE_SNAPRIGHT },
	{ "snap_up", RULE_SNAPUP },
	{ "snap_down", RULE_SNAPDOWN },
	{ "duplicate", RULE_DUPLICATE },
	{ "minimize", RULE_MINIMIZE },
};

// load a rule specified on cmd line or .goomwwmrc
void rule_parse(char *rulestr)
{
	winrule *new = allocate_clear(sizeof(winrule));
	char *str = strdup(rulestr); strtrim(str);
	char *left = str, *right = str;
	// locate end of pattern
	while (*right && !isspace(*right)) right++;
	if (right-left > RULEPATTERN-1)
	{
		fprintf(stderr, "rule exceeded pattern space: %s\n", str);
		free(new); free(str); return;
	}
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
			char flag[32]; memset(flag, 0, sizeof(flag));
			strncpy(flag, left, MIN(sizeof(flag)-1, right-left));
			// check for geometry
			if (regquick("^[0-9]*[%]*x[0-9]*[%]*$", flag))
			{
				new->flags |= RULE_SIZE;
				char *p = flag;
				new->w = strtol(p, &p, 10);
				new->w_is_pct = (*p == '%') ? 1:0;
				if (new->w_is_pct) p++;
				if (*p == 'x') p++;
				new->h = strtol(p, &p, 10);
				new->h_is_pct = (*p == '%') ? 1:0;
			} else
			// check known flags
			{
				int i; for (i = 0; i < sizeof(rulemap)/sizeof(winrulemap); i++)
					if (!strcasecmp(flag, rulemap[i].name))
						{ new->flags |= rulemap[i].flag; break; }
			}
		}
		// skip delimiters
		while (*right && strchr(" ,\t", *right)) right++;
	}
	// prepare pattern regexes
	char *pat = new->pattern;
	if (regquick("^(class|name|title):", pat)) pat = strchr(pat, ':')+1;

	if (regcomp(&new->re, pat, REG_EXTENDED|REG_ICASE|REG_NOSUB) == 0)
	{
		new->next = config_rules;
		config_rules = new;
	} else
	{
		fprintf(stderr, "failed to compile regex: %s\n", pat);
		free(new);
	}
	free(str);
}

// pick a ruleset to execute
void ruleset_switcher()
{
	int i, count = 0, current = 0; char **list; winruleset *set;

	// count rulesets
	for (count = 0, set = config_rulesets; set; count++, set = set->next);
	list = allocate_clear(sizeof(char*) * (count+1)); // +1 NULL sell terminates
	// find current selection
	for (current = 0, set = config_rulesets; set && set->rules != config_rules; current++, set = set->next);
	if (current == count) current = 0;
	// build a simple list of rule file names
	for (i = count-1, set = config_rulesets; set; i--, set = set->next) list[i] = basename(set->name);

	if (!fork())
	{
		display = XOpenDisplay(0);
		XSync(display, True);
		int n = menu(list, NULL, count-current-1);
		if (n >= 0 && list[n])
		{
			cli_message(gatoms[GOOMWWM_RULESET], list[n]);
			usleep(300000);
		}
		exit(EXIT_SUCCESS);
	}
	free(list);
}

// execute a ruleset on open windows
void ruleset_execute(char *name)
{
	int i; winruleset *set = NULL; Window w; client *c;

	// find ruleset by index
	for (set = config_rulesets; set && strcasecmp(name, set->name); set = set->next);

	if (set)
	{
		config_rules = set->rules;
		tag_ascend(i, w, c, current_tag) client_rules_apply(c);
	}
}