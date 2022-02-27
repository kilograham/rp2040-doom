#xxd -i tiny.wad | sed "s/unsigned/const unsigned/g" >src/tiny.wad.h
xxd -i tiny.whd | sed "s/unsigned/const unsigned/g" >src/tiny.whd.h
#cp tiny.wad.meta.h src/
echo dont forget
echo picotool load -v -n -t bin tiny.whd -o 0x10040000
