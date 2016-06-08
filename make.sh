
cd modules
(cd bs && sh make.sh)
(cd global && sh make.sh)
(cd str && sh make.sh)
(cd system && sh make.sh)
(cd tk && sh make.sh)
(cd vec && sh make.sh)
(cd random && sh make.sh)
(cd math && sh make.sh)
(cd cmath && sh make.sh)
(cd sys && sh make.sh)
(cd time && sh make.sh)
(cd os && sh make.sh)
(cd ma && sh make.sh)
(cd gx && sh make.sh)

cd ../objects
(cd string && sh make.sh)
(cd map && sh make.sh)
(cd list && sh make.sh)
(cd complex && sh make.sh)
(cd set && sh make.sh)
(cd function && sh make.sh)
(cd longgmp && sh make.sh)
(cd iterable && sh make.sh)

cd ..
(cd compiler && sh make.sh)
(cd vm && sh make.sh)

