includes="-I./source/ -L./build/"
cc="gcc"

mkdir -p ./build/int ./build/examples
rm -f ./build/int/*.o ./build/libcore.a

$cc -c ./source/edlib.c -o ./build/int/edlib.o $includes
ar rcs ./build/libedlib.a ./build/int/edlib.o

gcc ./examples/01-suite-test/main.c -o ./build/examples/01-test $includes -l:libedlib.a
