C# pong
left paddle w and s
right paddle up and down C#

screen w h

#C speed
set pspeed 6
set bspeed 5

set p1y 200
set p2y 200

set pad2x w
sub pad2x 40
set p2hit pad2x
sub p2hit 15

set maxy h
sub maxy 90
set bmax h
sub bmax 15
set s2x w
sub s2x 140

set s1 0
set s2 0
gosub serve

label loop
gosub input
gosub clamp
gosub move
gosub draw
wait 16
flip
goto loop

#C read keys and move the paddles
label input
key w k
if k = 0 goto i1
sub p1y pspeed
label i1
key s k
if k = 0 goto i2
add p1y pspeed
label i2
key up k
if k = 0 goto i3
sub p2y pspeed
label i3
key down k
if k = 0 goto i4
add p2y pspeed
label i4
return

#C keep the paddles on screen
label clamp
if p1y > 0 goto c1
set p1y 0
label c1
if p1y < maxy goto c2
set p1y maxy
label c2
if p2y > 0 goto c3
set p2y 0
label c3
if p2y < maxy goto c4
set p2y maxy
label c4
return

label move
add bx dx
add by dy

#C bounce off top and bottom
if by > 0 goto m1
mul dy -1
add by dy
label m1
if by < bmax goto m2
mul dy -1
add by dy
label m2

#C left paddle
if bx > 40 goto m3
if by < p1y goto m3
set t p1y
add t 90
if by > t goto m3
mul dx -1
set bx 41
label m3

#C right paddle
if bx < p2hit goto m4
if by < p2y goto m4
set t p2y
add t 90
if by > t goto m4
mul dx -1
set bx p2hit
sub bx 1
label m4

#C ball out on either side scores for the other player
if bx > 0 goto m5
add s2 1
gosub serve
label m5
if bx < w goto m6
add s1 1
gosub serve
label m6
return

C# ball back to the middle after a score C#
label serve
set bx w
div bx 2
set by h
div by 2
set dx bspeed
set dy bspeed
return

label draw
rect 20 p1y 15 90 0xFFFFFF
rect pad2x p2y 15 90 0xFFFFFF
rect bx by 15 15 0x00FF00
num 100 10 s1
num s2x 10 s2
return

end
