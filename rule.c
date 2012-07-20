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

// load a rule specified on cmd line or .goomwwmrc
int rule_parse(char *rulestr)
{
	winrule *new = allocate_clear(sizeof(winrule));
	char *str = strdup(rulestr); strtrim(str);
	char *left = str, *right = str;
	// locate end of pattern
	while (*right && !isspace(*right)) right++;
	if (right-left > RULEPATTERN-1)
	{
		fprintf(stderr, "rule exceeded pattern space: %s\n", str);
		free(new); free(str); return 0;
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

	int ok = 0;
	if (regcomp(&new->re, pat, REG_EXTENDED|REG_ICASE|REG_NOSUB) == 0)
	{
		new->next = config_rules;
		config_rules = new;
		ok = 1;
	} else
	{
		fprintf(stderr, "failed to compile regex: %s\n", pat);
		free(new);
	}
	free(str);
	return ok;
}

// pick a ruleset to execute
void ruleset_switcher()
{
	int i, count = 0; char **list; winruleset *set;

	// count rulesets
	for (count = 0, set = config_rulesets; set; count++, set = set->next);
	list = allocate_clear(sizeof(char*) * (count+1)); // +1 NULL sell terminates
	// build a simple list of rule file names
	for (i = count-1, set = config_rulesets; set; i--, set = set->next) list[i] = basename(set->name);

	if (!fork())
	{
		display = XOpenDisplay(0);
		XSync(display, True);
		int n = menu(list, NULL, 0);
		if (n >= 0 && list[n])
		{
			cli_message(gatoms[GOOMWWM_RULESET], list[n]);
			usleep(300000);
		}
		exit(EXIT_SUCCESS);
	}
	free(list);
}

// apply a rule list to all windows in current_tag
void rulelist_apply(winrule *list)
{
	int i; Window w; client *c;
	winrule *bak = config_rules; config_rules = list;
	tag_ascend(i, w, c, current_tag)
	{
		winlist_empty(cache_xattr);
		winlist_empty(cache_client);
		c = client_create(w);
		client_raise(c, 0);
		if (c) client_rules_apply(c);
		XSync(display, False);
	}
	clients_ascend(windows_minimized, i, w, c)
		if (c->manage && c->cache->tags & current_tag)
	{
		winlist_empty(cache_xattr);
		winlist_empty(cache_client);
		c = client_create(w);
		if (c) client_rules_apply(c);
		XSync(display, False);
	}
	config_rules = bak;
}

// apply a single rule to all windows in the current tag
void rule_apply(winrule *rule)
{
	winrule *next = rule->next;
	rule->next = NULL;
	rulelist_apply(rule);
	rule->next = next;
}

// execute a ruleset on open windows
void ruleset_execute(char *name)
{
	winruleset *set = NULL;
	// find ruleset by index
	for (set = config_rulesets; set && strcasecmp(name, set->name); set = set->next);
	if (set)
	{
		// bit odd. rules lists are lifos present, but it's more intuitive to process
		// rulesets in the order they were defined. should clean this up, but for now,
		// labouriously walk the list backwards
		winrule *rule = set->rules;
		while (rule->next) rule = rule->next;
		while (rule)
		{
			rule_apply(rule);
			winrule *prev = set->rules;
			while (prev && prev->next != rule) prev = prev->next;
			rule = prev;
		}
	}
}