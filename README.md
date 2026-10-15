# Jedy

Jedy is a simple, terminal based text editor, written in C++. It tries to mimic the capabilities
of Vim to a certain level, which is my needs from a text editor.

Jedy is a state based text editor, just like Vim. There are a couple modes you can be in, those are
as follows:

- Selection
- Navigation
- Input
- Command
- Search

with Navigation mode being the default and central mode.

## Navigation Mode

In navigation mode, you can navigate through the buffer using the classic Vim
keybindings: H (left), J (down), K (up), and L (right). These are the main
navigation keys, with a couple more keybindings to help you move around more
freely:

- } : Skip a block of text forward
- { : Skip a block of text backward
- ] : Skip to the end of the file
- [ : Skip to the start of the file
- $ : Skip to the end of the line
- 0 : Skip to the start of the line
- w : Skip a word forward
- b : Skip a word backward
- m : Mark/Unmark current line
- n : Jump to next mark
- N : Jump to previous mark

Through navigation mode you can enter the other modes through the given keybindings:

- i : Input mode
- v : Selection mode
- : : Command mode
- / : Search mode

After entering any of these modes, you can exit and return back to Navigation mode using ESC.

## Input Mode

Input mode is just as it sounds, it takes your input and writes to the buffer. Be aware that changes
made are local and temporary unless you flush them using Command mode.

## Selection Mode

In selection mode, you can move around using the same bindings from the Navigation mode. By moving around,
you'll select some text with your cursor, the selected text will be highlighted. You can use the following
keybindings to manipulate the selected text:

- P : Paste from the copy buffer, replacing the selected text
- p : Pase from the copy buffer, prepending to the cursor
- D : Delete selected text, do not copy into the copy buffer
- d : Delete selected text, copying into the copy buffer
- y : Copy selected text into the copy buffer
- < : Shift each selected line one TAB to left
- > : Shift each selected line one TAB to right

## Command Mode

A couple of commands are available, some of them with the chaining capabilities:

- q : Close the window, any unsaved change is lost, kills all open buffers, chainable
- w : Flush the current buffer to its file, chainable
- l : Skip to line
- p : Switch to buffer
- k : Kill the current buffer, any unsaved change is lost
- e : Open/Reopen file

## Search Mode

In search mode, there are two options, you can either search forward or search backward.

To search forward, start your query with 'f' like:

`/f Something` or `/fSomething`

To jump to the next match of the query, press TAB whilst in the Search mode.

To search backward, start your query with 'b' like:

`/b Something` or `/bSomething`

To jump to the next match of the query, again, press TAB whilst in the Search mode.