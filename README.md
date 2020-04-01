# xcb_xlib
use xcb or xlib to record Linux Screen
####
firstly, please use commands as follow:
mkdir build
cd build
cmake ..
secondly,can produce Xcb and Xlib

###
Error:
E1:Can't open X display(use A login,but su B in command)
   解决办法：
   1在root用户(A)下先执行xhost local:sh
   2 切换到sh用户：su sh
   3 再次执行程序就可以了
