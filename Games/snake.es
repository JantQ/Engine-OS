#C snake

screen w h
set cell 20
set gw w
div gw cell
set gh h
div gh cell

#C body array
alloc bx 500
alloc by 500

set len 3
set hx gw
div hx 2
set hy gh
div hy 2
set dx 1
set dy 0

gosub initbody
gosub newfood

label loop
gosub input
gosub move
gosub draw
wait 110
flip
goto loop

label initbody
set i 0
label ib1
set t hx
sub t i
poke bx i t
poke by i hy
add i 1
if i < len goto ib1
return

#C read keys
label input
key right k
if k = 0 goto k1
if dx = -1 goto k1
set dx 1
set dy 0
label k1
key left k
if k = 0 goto k2
if dx = 1 goto k2
set dx -1
set dy 0
label k2
key up k
if k = 0 goto k3
if dy = 1 goto k3
set dx 0
set dy -1
label k3
key down k
if k = 0 goto k4
if dy = -1 goto k4
set dx 0
set dy 1
label k4
return

label move
gosub shiftbody
add hx dx
add hy dy
poke bx 0 hx
poke by 0 hy
gosub walls
gosub selfbite
gosub food
return


label shiftbody
set i len
sub i 1
label sh1
if i < 1 goto sh2
set j i
sub j 1
peek bx j t
poke bx i t
peek by j t
poke by i t
sub i 1
goto sh1
label sh2
return

label walls
if hx < 0 goto dead
if hy < 0 goto dead
if hx < gw goto w1
goto dead
label w1
if hy < gh goto w2
goto dead
label w2
return

label selfbite
set i 1
label sb1
if i < len goto sb2
return
label sb2
peek bx i t
if t ! hx goto sb3
peek by i t
if t = hy goto dead
label sb3
add i 1
goto sb1

label food
if hx ! fx goto f1
if hy ! fy goto f1
set k len
sub k 1
peek bx k tx
peek by k ty
poke bx len tx
poke by len ty
add len 1
if len < 500 goto f2
set len 499
label f2
gosub newfood
label f1
return

label newfood
rnd fx gw
rnd fy gh
return

label draw
set px fx
mul px cell
set py fy
mul py cell
rect px py cell cell 0xFF0000
set i 0
label d1
if i < len goto d2
goto d3
label d2
peek bx i t
mul t cell
set u 0
peek by i u
mul u cell
rect t u cell cell 0x00FF00
add i 1
goto d1
label d3
set sc len
sub sc 3
num 10 10 sc
return

label dead
text 100 100 "GAME OVER"
num 100 140 sc
flip
wait 2000
end
