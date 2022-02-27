xxd -i $1 | sed "s/unsigned/const unsigned/g" | sed "s/$1/tiny_whd/g" >src/tiny.whd.h
