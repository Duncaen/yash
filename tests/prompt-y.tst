# prompt-y.tst: yash-specific test of input processing

mkfifo fifo

(

if [ "$(id -u)" -eq 0 ]; then
    skip="true"
fi

(
# Detail behavior of prompting and command history is different among shell
# implementations, so we don't test it in input-p.tst.
posix="true"

(
setup -d

test_o 'default prompt strings (POSIX)' -i +m
bracket "$PS1"
bracket "$PS2"
bracket "$PS4"
__IN__
[$ ]
[> ]
[+ ]
__OUT__

)

test_e 'expansion in PS1 (POSIX)' -i +m
PS1='ps1 %'; a=A; echo >&2
PS1='${a} @'; echo >&2
PS1=''; echo >&2
__IN__
$ 
ps1 %
A @
__ERR__

test_e 'PS2' -i +m
PS2='${b} %'; \
echo >&2
b=B; \
echo >&2
true &&
echo >&2; exit
__IN__
$ > 
$  %
$ B %
__ERR__

# It is POSIXly unspecified (1) whether assignment to $PS4 affects the prompt
# for the assignment itself, and (2) how characters are quoted in xtrace. This
# test case tests yash-specific behavior.
test_e 'PS4' -x
echo 0
PS4='ps4 ${x}:' x='${y}'
echo 1; echo '2  2' 3
__IN__
+ echo 0
ps4 ${y}:PS4=ps4 ${x}: x=${y}
ps4 ${y}:echo 1
ps4 ${y}:echo 2  2 3
__ERR__

(
if ! "$TESTEE" -c 'command -bv fc history' >/dev/null; then
    skip="true"
fi

export HISTFILE=/dev/null HISTRMDUP=1

test_e 'job number expansion in PS1' -i +m
PS1='#!$'; echo >&2
echo >&2
echo >&2
echo >&2; exit
__IN__
$ 
#2$
#3$
#3$
__ERR__

test_e 'literal exclamation in PS1' -i +m
PS1='!!$'; echo >&2
echo >&2
echo >&2
echo >&2; exit
__IN__
$ 
!$
!$
!$
__ERR__

)

)

test_e 'expansion and substitution in PS1' -i +m
PS1='${PWD##"$PWD"}$(echo \?)'; echo >&2
PS1='! !! $ '; echo >&2
echo >&2; exit
__IN__
$ 
?
! !! $ 
__ERR__

# TODO: Test of \[, \], and \f is missing
# \j and \$ are tested in other test cases below
test_e 'backslash notations in PS1' -i +m
PS1='\a \e \n \r $(printf \\\\)\ $'; echo >&2
echo >&2; exit
__IN__
$ 
  
  \ $
__ERR__

# TODO: Test of \j, \[, \], and \f is missing
# \$ is tested in another test case below
test_e 'backslash notations in PS2' -i +m
PS2='\a \e \n \r \\ >'; echo >&2
\
echo >&2; exit
__IN__
$ 
$   
  \ >
__ERR__

test_e '\j in PS1: shows job count' -i +m
PS1='\j$'; echo >&2
:&         echo >&2
:&&:&      echo >&2
wait $!;   echo >&2
wait;      echo >&2
           echo >&2; exit
__IN__
$ 
0$
1$
2$
1$
0$
__ERR__

test_e '\j in PS1 and -b option' -ib +m
PS1='\j$';  echo >&2
exec >fifo& echo >&2
cat fifo
wait $!;    echo >&2
            echo >&2; exit
__IN__
$ 
0$
1$[1] + Done                 exec 1>fifo
0$
0$
__ERR__

user_id="$(id -u)"

(
if [ "$user_id" -ne 0 ]; then
    skip="true"
fi

test_e '\$ in PS1 and PS2 (root)' -i +m
PS1='\$ ' PS2='\$_'; echo >&2
e\
c\
ho >&2; exit
__IN__
$ 
# #_#_
__ERR__

)

(
if [ "$user_id" -eq 0 ]; then
    skip="true"
fi

test_e '\$ in PS1 and PS2 (non-root)' -i +m
PS1='\$ ' PS2='\$_'; echo >&2
e\
c\
ho >&2; exit
__IN__
$ 
$ $_$_
__ERR__

)

test_e 'prompt command' -i +m
PROMPT_COMMAND='printf 1 >&2'; echo >&2
PROMPT_COMMAND=('printf 1 >&2'
'printf 2 >&2; printf 3 >&2; (exit 2)'); echo $? >&2; (exit 1)
echo $? >&2; exit
__IN__
$ 
1$ > 0
123$ 1
__ERR__

)

(
setup -d

(
if [ "$(id -u)" -ne 0 ]; then
    skip="true"
fi

posix="true"

test_o 'default prompt strings (POSIX, root)' -i +m
bracket "$PS1"
bracket "$PS2"
bracket "$PS4"
__IN__
[# ]
[> ]
[+ ]
__OUT__

)

test_o 'default prompt strings' -i +m
bracket "$PS1"
bracket "$PS2"
bracket "$PS4"
__IN__
[\$ ]
[> ]
[+ ]
__OUT__

)

# vim: set ft=sh ts=8 sts=4 sw=4 noet:
