# badavi

[![Build Status](https://travis-ci.org/isbadawi/badavi.svg?branch=master)](https://travis-ci.org/isbadawi/badavi)

`badavi` is a vi-like terminal mode text editor, implemented in C and using the
[`termbox`](https://github.com/nsf/termbox) library to draw to the terminal.

It's meant to be a learning exercise and fun project to hack on rather than a
serious day-to-day editor, although who knows where it'll end up.

### Features supported so far

* Basic motions -- `h`, `j`, `k`, `l`, `0`, `$`, `^`, `{`, `}`, `b`, `B`,
`w`, `W`, `e`, `E`, `G`, `g_`, `ge`, `gE`, `gg`. Motions can be prefixed with
an optional count.

* Basic `:` commands -- `:w [filename]` to save, `:wq` to save and quit, `:q`
to quit, `:q!` to force quit even if there are unsaved changes, and `:e path`
to edit another file.

* Some of the simpler options are implemented, e.g. `number`, `relativenumber`,
`numberwidth`, `ignorecase`, `cursorline`. You can use the same `:set` commands
as in vim, e.g. `:set nonumber`, `:set numberwidth=8`, etc. You can also put
those commands in a `~/.badavimrc` file to execute them on startup.

* Delete (`d`), change (`c`) and yank (`y`) operators, which can be applied to
any of the motions. (Text objects aren't implemented yet). The affected
region is saved into the unnamed register, used by `p` to paste text. Named
registers from `a` to `z` are also implemented, and can be specified by
prefixing the operator (or `p`) with `"a` through `"z`.

* Search forwards with `/`, backwards with `?`. Standard POSIX regexes are
used, so the syntax is not exactly the same as vim's. For instance, word
boundaries are specified with `[[:<:]]` and `[[:>:]]` instead of `\<` and
`\>`. `n` and `N` can be used to cycle through matches. `*` and `#` can be
used to search forwards or backwards for the next occurrence of the word
under the cursor.

* Undo (`u`) and redo (`<c-r>`) (only single-level for now, unlike vim).

* `ctags` support -- on startup badavi looks for a tags file called `tags` in
the current directory (`tags` option not supported yet). The `:tag` command
jumps to the specified tag, and `<c-]>` jumps to the tag of the word under
the cursor. `<c-t>` and `:tag` can be used to walk up and down the tag stack.
The `-t` command line option can also be passed in to start editing at the
given tag, as in e.g. `badavi -t main`.

### Building

Just run `make`. The only dependency is `termbox`, which is easy to find --
packages surely exist for your favorite OS. On OS X, `brew install termbox`
will do.

### License

MIT -- see `LICENSE` file for details.
