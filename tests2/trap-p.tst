# trap-p.tst: test of the trap built-in for any POSIX-compliant shell

posix="true"

test_OE -e USR1 'setting default trap'
trap - USR1
kill -s USR1 $$
__IN__

test_OE -e 0 'setting ignore trap'
trap '' USR1
kill -s USR1 $$
(kill -s USR1 $$)
__IN__

test_oE -e 0 'setting command trap'
trap 'echo trap; echo executed' USR1
kill -s USR1 $$
__IN__
trap
executed
__OUT__

test_OE -e USR1 'resetting to default trap'
trap '' USR1
trap - USR1
kill -s USR1 $$
__IN__

test_oE -e 0 'specifying multiple signals'
trap 'echo trapped' USR1 USR2
kill -s USR1 $$
kill -s USR2 $$
__IN__
trapped
trapped
__OUT__

# $1 = $LINENO, $2 = signal number, $3 = signal name w/o SIG-prefix
test_specifying_signal_by_number() {
    testcase "$1" -e 0 "specifying signal by number ($3)" \
	3<<__IN__ 4<<__OUT__ 5</dev/null
trap 'echo trapped' $2
kill -s $3 \$\$
__IN__
trapped
__OUT__
}

test_specifying_signal_by_number "$LINENO" 1  HUP
test_specifying_signal_by_number "$LINENO" 2  INT
test_specifying_signal_by_number "$LINENO" 3  QUIT
test_specifying_signal_by_number "$LINENO" 6  ABRT
#test_specifying_signal_by_number "$LINENO" 9  KILL
test_specifying_signal_by_number "$LINENO" 14 ALRM
test_specifying_signal_by_number "$LINENO" 15 TERM

test_OE -e INT 'initial numeric operand implies default trap (first operand)'
trap 'echo trapped' 2 QUIT
trap 2 QUIT
kill -s INT $$
__IN__

test_OE -e QUIT 'initial numeric operand implies default trap (second operand)'
trap 'echo trapped' 2 QUIT
trap 2 QUIT
kill -s QUIT $$
__IN__

test_oE -e 0 'setting trap for EXIT (EOF)'
trap 'echo trapped; false' EXIT
echo exiting
__IN__
exiting
trapped
__OUT__

test_oE -e 7 'setting trap for EXIT (exit built-in)'
trap 'echo trapped; (exit 9)' EXIT
exit 7
__IN__
trapped
__OUT__

test_oE -e 0 '$? is restored after trap is executed'
trap 'false' USR1
kill -s USR1 $$
echo $?
__IN__
0
__OUT__

test_oE 'trap command is not affected by redirections effective when set' \
    -c 'trap "echo foo" EXIT >/dev/null'
__IN__
foo
__OUT__

test_oE 'command is evaluated each time trap is executed'
trap X USR1
alias X='echo 1'
kill -s USR1 $$
alias X='echo 2'
kill -s USR1 $$
__IN__
1
2
__OUT__

(
trap '' USR1 USR2

test_oE 'traps cannot be modified for initially ignored signal'
trap -              USR1 2>/dev/null
trap 'echo trapped' USR2 2>/dev/null
kill -s USR1 $$ # ignored
kill -s USR2 $$ # ignored
echo reached
__IN__
reached
__OUT__

)

test_oE -e 0 'single trap may be invoked more than once'
trap 'echo trapped' USR1
kill -s USR1 $$
(kill -s USR1 $$)
kill -s USR1 $$
__IN__
trapped
trapped
trapped
__OUT__

test_OE -e 0 'ignore trap is inherited to external command'
trap '' USR1
sh -c 'kill -s USR1 $$'
__IN__

test_oE -e 0 'command trap is reset in external command'
trap 'echo trapped' USR1
sh -c 'kill -s USR1 $$'
kill -l $?
__IN__
USR1
__OUT__

test_oE 'default traps remain in subshell'
trap - USR1
(sh -c 'kill -s USR1 $$')
kill -l $?
__IN__
USR1
__OUT__

test_OE -e 0 'ignored traps remain in subshell'
trap '' USR1
(sh -c 'kill -s USR1 $$')
__IN__

test_oE 'command traps are reset in subshell'
trap 'echo trapped' USR1
(sh -c 'kill -s USR1 $PPID'; :)
kill -l $?
__IN__
USR1
__OUT__

test_oE -e 0 'setting new trap in subshell'
trap 'echo X' USR1
(trap 'echo trapped' USR1; sh -c 'kill -s USR1 $PPID'; :)
__IN__
trapped
__OUT__

test_oE -e 0 'printing traps' -e
trap 'echo "a"'"'b'"'\c' USR1
trap >printed_trap
trap - USR1
. ./printed_trap
kill -s USR1 $$
__IN__
abc
__OUT__

test_oE -e 0 'traps are printed even in command substitution' -e
trap 'echo "a"'"'b'"'\c' USR1
printed_trap="$(trap)"
trap - USR1
eval "$printed_trap"
kill -s USR1 $$
__IN__
abc
__OUT__

echo 'echo "$@"' > ./-
chmod a+x ./-

test_oE 'setting command trap that starts with hyphen'
PATH=.:$PATH
trap -- '- trapped' USR1
kill -s USR1 $$
__IN__
trapped
__OUT__

test_o -d 'invalid signal is not syntax error'
trap '' '' || echo reached
__IN__
reached
__OUT__

# vim: set ft=sh ts=8 sts=4 sw=4 noet:
