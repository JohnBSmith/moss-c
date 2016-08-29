
cd ..
printf "C: "
find -name "*.c" | xargs cat | wc -l
printf "Moss: "
find -name "*.moss" | xargs cat | wc -l
